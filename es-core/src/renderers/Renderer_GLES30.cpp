#include "Renderer_GLES30.h"

#ifdef RENDERER_GLES_30

#include "Renderer_GLES30.h"
#include "renderers/Renderer.h"
#include "math/Transform4x4f.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"
#include "Settings.h"

#include <algorithm>
#include <array>
#include <vector>
#include <set>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifndef NDEBUG
#define GLES30_CALL(Function) GL_CHECK_ERROR(Function)
#else
#define GLES30_CALL(Function) (Function)
#endif

#include "GlExtensions.h"
#include "Shader.h"

#include "resources/ResourceManager.h"

namespace Renderer
{

//////////////////////////////////////////////////////////////////////////

	struct TextureInfo
	{
		GLenum type;
		Vector2f size;
		unsigned int mipLevels = 1;
		size_t memorySize = 0;
	};

	static SDL_GLContext	sdlContext       = nullptr;

	static Transform4x4f	projectionMatrix = Transform4x4f::Identity();
	static Transform4x4f	worldViewMatrix  = Transform4x4f::Identity();
	static Transform4x4f	mvpMatrix		 = Transform4x4f::Identity();
	static bool			mvpMatrixDirty  = true;

	static ShaderProgram    shaderProgramColorTexture;
	static ShaderProgram    shaderProgramColorTextureFast;
	static ShaderProgram    shaderProgramColorNoTexture;
	static ShaderProgram    shaderProgramAlpha;

	static GLuint			vertexBuffer     = 0;

	struct StreamRange
	{
		GLsizei vertexCount = 0;
		uint64_t generation = 0;
		bool valid = false;
	};

	static StreamRange lastStreamRange;
	static std::vector<Vertex> lastLogicalVertices;
	static uint64_t streamBufferGeneration = 1;

	// Reused across every upload so the hot path never touches the allocator.
	static std::vector<GpuVertex> packedScratch;

	struct PixelUploadSlot
	{
		GLuint buffer = 0;
		GLsync fence = nullptr;
		size_t capacity = 0;
	};

	static std::array<PixelUploadSlot, 3> pixelUploadSlots;
	static const size_t PIXEL_UPLOAD_THRESHOLD = 256 * 1024;

	enum class QuadBatchShader
	{
		NO_TEXTURE,
		COLOR_TEXTURE
	};

	struct QuadBatch
	{
		// Four vertices per quad, in triangle-strip order.
		std::vector<Vertex> vertices;
		unsigned int texture = 0;
		Blend::Factor sourceBlend = Blend::SRC_ALPHA;
		Blend::Factor destinationBlend = Blend::ONE_MINUS_SRC_ALPHA;
		QuadBatchShader shader = QuadBatchShader::NO_TEXTURE;
		bool active = false;
	};

	struct GLES30PerformanceCounters
	{
		uint64_t submittedQuads = 0;
		uint64_t batchFlushes = 0;
		uint64_t singleQuadFlushes = 0;
		uint64_t indexedFlushes = 0;
		uint64_t pboUploads = 0;
		uint64_t pboFallbacks = 0;
	};

	static QuadBatch quadBatch;
	static GLES30PerformanceCounters performanceCounters;

	static const size_t MAX_BATCH_QUADS = 1024;
	static const size_t MAX_BATCH_VERTICES = MAX_BATCH_QUADS * 4;

	static GLuint quadIndexBuffer = 0;

	static void flushQuadBatch();

//////////////////////////////////////////////////////////////////////////

	static GLCapabilities glCapabilities;

	const GLCapabilities& getGLCapabilities() { return glCapabilities; }

	struct ESContextVersion
	{
		int major;
		int minor;
	};

	// Highest first.
	static const ESContextVersion ES_CONTEXT_LADDER[] = { { 3, 2 }, { 3, 1 }, { 3, 0 } };
	static const int ES_CONTEXT_LADDER_COUNT = sizeof(ES_CONTEXT_LADDER) / sizeof(ES_CONTEXT_LADDER[0]);

	static bool hasExtension(const std::set<std::string>& extensions, const char* name)
	{
		return extensions.find(name) != extensions.cend();
	}

	void detectGLCapabilities()
	{
		glCapabilities = GLCapabilities();

		// Reports what the driver created, which may exceed what was requested.
		GLint major = 0;
		GLint minor = 0;
		while (glGetError() != GL_NO_ERROR)
			;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
		if (glGetError() == GL_NO_ERROR && major >= 3)
		{
			glCapabilities.majorVersion = major;
			glCapabilities.minorVersion = minor;
		}

		std::set<std::string> extensions;
		GLint extensionCount = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
		for (GLint i = 0; i < extensionCount; ++i)
		{
			const GLubyte* extension = glGetStringi(GL_EXTENSIONS, i);
			if (extension != nullptr)
				extensions.insert(reinterpret_cast<const char*>(extension));
		}
		while (glGetError() != GL_NO_ERROR)
			;

		glCapabilities.debugOutput = glCapabilities.isAtLeast(3, 2) || hasExtension(extensions, "GL_KHR_debug");

		LOG(LogInfo) << "GL ES context: " << glCapabilities.majorVersion << "." << glCapabilities.minorVersion;
		LOG(LogInfo) << " KHR_debug:    " << (glCapabilities.debugOutput ? "yes" : "no");
	}

	struct GLES30StateCache
	{
		bool blendKnown = false;
		bool blendEnabled = false;
		bool blendFactorsKnown = false;
		GLenum blendSource = GL_ONE;
		GLenum blendDestination = GL_ZERO;

		bool scissorKnown = false;
		bool scissorEnabled = false;
		bool scissorRectKnown = false;
		Rect scissorRect;

		bool viewportKnown = false;
		Rect viewport;
	};

	static GLES30StateCache stateCache;

	static std::unordered_map<unsigned int, TextureInfo*> _textures;

	static unsigned int		boundTexture = 0;
	// Resolved alongside boundTexture to keep it out of the per-draw path.
	static TextureInfo*		boundTextureInfo = nullptr;
	static unsigned int		mShaderTexture = 0;

	extern std::string SHADER_VERSION_STRING;

//////////////////////////////////////////////////////////////////////////

	static ShaderProgram* currentProgram = nullptr;

	static void useProgram(ShaderProgram* program)
	{
		if (program == currentProgram)
		{
			if (currentProgram != nullptr && mvpMatrixDirty)
			{
				currentProgram->setMatrix(mvpMatrix);
				mvpMatrixDirty = false;
			}

			return;
		}

		if (program == nullptr && currentProgram != nullptr)
			currentProgram->unSelect();

		currentProgram = program;

		if (currentProgram != nullptr)
		{
			currentProgram->select(vertexBuffer, quadIndexBuffer);
			currentProgram->setMatrix(mvpMatrix);
			mvpMatrixDirty = false;
		}
	}

	static std::map<std::string, ShaderProgram*> _customShaders;

	static ShaderProgram* getShaderProgram(const char* shaderFile)
	{
		if (shaderFile == nullptr || strlen(shaderFile) == 0)
			return nullptr;

		auto it = _customShaders.find(shaderFile);
		if (it != _customShaders.cend())
			return it->second;

		ShaderProgram* customShader = new ShaderProgram();
		if (!customShader->loadFromFile(shaderFile))
		{
			delete customShader;
			customShader = nullptr;
		}

		_customShaders[shaderFile] = customShader;

		return customShader;
	}


	class ShaderBatch : public std::vector<ShaderProgram*>
	{
	public:
		static ShaderBatch* getShaderBatch(const char* shaderFile);

		std::map<std::string, std::string> parameters;
	};

	static std::map<std::string, ShaderBatch*> _customShaderBatch;

	ShaderBatch* ShaderBatch::getShaderBatch(const char* shaderFile)
	{
		if (shaderFile == nullptr)
			return nullptr;

		auto it = _customShaderBatch.find(shaderFile);
		if (it != _customShaderBatch.cend())
			return it->second;

		std::string fullPath = ResourceManager::getInstance()->getResourcePath(shaderFile);

		ShaderBatch* ret = new ShaderBatch();

		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fullPath));
		if (ext == ".glslp")
		{
			std::string path = Utils::FileSystem::getParent(fullPath);

			std::map<std::string, std::string> confMap;

			std::string line;
			std::ifstream systemConf(fullPath);
			if (systemConf && systemConf.is_open())
			{
				while (std::getline(systemConf, line))
				{
					int idx = line.find("=");
					if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
						continue;

					std::string key = Utils::String::trim(line.substr(0, idx));
					std::string value = Utils::String::trim(Utils::String::replace(line.substr(idx + 1), "\"", ""));
					if (!key.empty() && !value.empty())
						confMap[key] = value;

				}
				systemConf.close();
			}

			int count = 0;

			auto it = confMap.find("shaders");
			if (it != confMap.cend())
				count = Utils::String::toInteger(it->second);

			for (int i = 0; i < count; i++)
			{
				auto name = "shader" + std::to_string(i);

				it = confMap.find(name);
				if (it == confMap.cend())
					continue;

				std::string relative = it->second;
				if (!Utils::String::startsWith(relative, ":") && !Utils::String::startsWith(relative, "/") && !Utils::String::startsWith(relative, "."))
					relative = "./" + relative;

				std::string full = Utils::FileSystem::resolveRelativePath(relative, path, true);

				ShaderProgram* customShader = getShaderProgram(full.c_str());
				if (customShader != nullptr)
					ret->push_back(customShader);
			}

			it = confMap.find("parameters");
			if (it != confMap.cend())
			{
				for (auto prm : Utils::String::split(it->second, ';', true))
				{
					it = confMap.find(prm);
					if (it != confMap.cend())
						ret->parameters[prm] = it->second;
				}
			}
		}
		else
		{
			ShaderProgram* customShader = getShaderProgram(fullPath.c_str());
			if (customShader != nullptr)
				ret->push_back(customShader);
		}

		_customShaderBatch[shaderFile] = ret;
		return ret;
	};

	static int getAvailableVideoMemory();

	static void setupDefaultShaders()
	{
#if defined(USE_OPENGLES_30)
		SHADER_VERSION_STRING = "#version 300 es\n";
#elif defined(USE_OPENGLES_20)
		SHADER_VERSION_STRING = "#version 100\n";
#else
		SHADER_VERSION_STRING = "#version 120\n";

		std::string shaders = Utils::String::trim(glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "");

		auto sep = shaders.find_first_of(" -");
		if (sep != std::string::npos)
			shaders = shaders.substr(0, sep);

		if (shaders.find("1.0") != std::string::npos)
			SHADER_VERSION_STRING = "#version 100\n";
		else if (shaders.find("1.1") != std::string::npos)
			SHADER_VERSION_STRING = "#version 110\n";
#endif

		LOG(LogInfo) << "GLSL version preprocessor :     " << SHADER_VERSION_STRING;

		const std::string vertexCompatibility = R"=====(
			#if __VERSION__ >= 130
			#define COMPAT_ATTRIBUTE in
			#define COMPAT_VARYING out
			#else
			#define COMPAT_ATTRIBUTE attribute
			#define COMPAT_VARYING varying
			#endif
		)=====";

		const std::string fragmentCompatibility = R"=====(
			#if __VERSION__ >= 130
			#define COMPAT_VARYING in
			#define COMPAT_TEXTURE texture
			#ifdef GL_ES
			out mediump vec4 FragColor;
			#else
			out vec4 FragColor;
			#endif
			#else
			#define COMPAT_VARYING varying
			#define COMPAT_TEXTURE texture2D
			#define FragColor gl_FragColor
			#endif
			#if __VERSION__ >= 300
			#define COMPAT_ALPHA(value) value.r
			#else
			#define COMPAT_ALPHA(value) value.a
			#endif
		)=====";

		// vertex shader (no texture)
		std::string vertexSourceNoTexture =
			SHADER_VERSION_STRING + vertexCompatibility +
			R"=====(
			uniform   mat4 MVPMatrix;
			COMPAT_ATTRIBUTE vec2 VertexCoord;
			COMPAT_ATTRIBUTE vec4 COLOR;
			COMPAT_VARYING   vec4 v_col;
			void main(void)
			{
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_col       = COLOR;
			}
			)=====";

		// fragment shader (no texture)
		std::string fragmentSourceNoTexture =
			SHADER_VERSION_STRING + fragmentCompatibility +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			#endif

			COMPAT_VARYING vec4 v_col;

			void main(void)
			{
			    FragColor = v_col;
			}
			)=====";

		// Compile each shader, link them to make a full program
		auto vertexShaderNoTexture = Shader::createShader(GL_VERTEX_SHADER, vertexSourceNoTexture);
		auto fragmentShaderColorNoTexture = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceNoTexture);

		shaderProgramColorNoTexture.createShaderProgram(vertexShaderNoTexture, fragmentShaderColorNoTexture);

		// vertex shader (texture)
		std::string vertexSourceTexture =
			SHADER_VERSION_STRING + vertexCompatibility +
			R"=====(
			uniform   mat4 MVPMatrix;
			COMPAT_ATTRIBUTE vec2 VertexCoord;
			COMPAT_ATTRIBUTE vec2 TexCoord;
			COMPAT_ATTRIBUTE vec4 COLOR;
			COMPAT_VARYING   vec2 v_tex;
			COMPAT_VARYING   vec4 v_col;
			COMPAT_VARYING   vec2 v_pos;

			void main(void)
			{
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_tex       = TexCoord;
			    v_col       = COLOR;
			    v_pos       = VertexCoord;
			}
			)=====";

		// fragment shader (texture)
		std::string fragmentSourceTexture =
			SHADER_VERSION_STRING + fragmentCompatibility +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			precision mediump sampler2D;
			#endif

			COMPAT_VARYING vec4 v_col;
			COMPAT_VARYING vec2 v_tex;
			COMPAT_VARYING vec2 v_pos;

			uniform sampler2D u_tex;
			uniform vec2      outputSize;
			uniform vec2      outputOffset;
			uniform float     saturation;
			uniform float     es_cornerRadius;

			void main(void)
			{
			    vec4 clr = COMPAT_TEXTURE(u_tex, v_tex);

			    if (saturation != 1.0) {
			        vec3 gray = vec3(dot(clr.rgb, vec3(0.34, 0.55, 0.11)));
			        vec3 blend = mix(gray, clr.rgb, saturation);
			        clr = vec4(blend, clr.a);
			    }

			    if (es_cornerRadius != 0.0) {
			        vec2 pos = abs(v_pos - outputOffset);
			        vec2 middle = vec2(abs(outputSize.x), abs(outputSize.y)) / 2.0;
			        vec2 center = abs(v_pos - outputOffset - middle);
			        vec2 q = center - middle + es_cornerRadius;
			        float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - es_cornerRadius;

			        if (distance > 0.0) {
			            discard;
			        }
			        else if (pos.x >= 1.0 && pos.y >= 1.0 && pos.x <= outputSize.x - 1.0 && pos.y <= outputSize.y - 1.0) {
			            float pixelValue = 1.0 - smoothstep(-0.75, 0.5, distance);
			            clr.a *= pixelValue;
			        }
			    }

			    FragColor = clr * v_col;
			}
			)=====";

		// Compile each shader, link them to make a full program
		auto vertexShaderTexture = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTexture);
		auto fragmentShaderColorTexture = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceTexture);
		shaderProgramColorTexture.createShaderProgram(vertexShaderTexture, fragmentShaderColorTexture);

		// Common 4K artwork path: avoid saturation, rounded-corner, position-varying,
		// and dynamic-branch work when no effect requests those features.
		std::string vertexSourceTextureFast =
			SHADER_VERSION_STRING + vertexCompatibility +
			R"=====(
			uniform   mat4 MVPMatrix;
			COMPAT_ATTRIBUTE vec2 VertexCoord;
			COMPAT_ATTRIBUTE vec2 TexCoord;
			COMPAT_ATTRIBUTE vec4 COLOR;
			COMPAT_VARYING   vec2 v_tex;
			COMPAT_VARYING   vec4 v_col;

			void main(void)
			{
			    gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
			    v_tex       = TexCoord;
			    v_col       = COLOR;
			}
			)=====";

		std::string fragmentSourceTextureFast =
			SHADER_VERSION_STRING + fragmentCompatibility +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			precision mediump sampler2D;
			#endif

			COMPAT_VARYING vec4 v_col;
			COMPAT_VARYING vec2 v_tex;
			uniform sampler2D u_tex;

			void main(void)
			{
			    FragColor = COMPAT_TEXTURE(u_tex, v_tex) * v_col;
			}
			)=====";

		auto vertexShaderTextureFast = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTextureFast);
		auto fragmentShaderColorTextureFast = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceTextureFast);
		shaderProgramColorTextureFast.createShaderProgram(vertexShaderTextureFast, fragmentShaderColorTextureFast);

		// fragment shader (alpha texture)
		std::string fragmentSourceAlpha =
			SHADER_VERSION_STRING + fragmentCompatibility +
			R"=====(
			#ifdef GL_ES
			precision mediump float;
			precision mediump sampler2D;
			#endif

			COMPAT_VARYING vec4 v_col;
			COMPAT_VARYING vec2 v_tex;
			uniform sampler2D u_tex;

			void main(void)
			{
			    float alpha = COMPAT_ALPHA(COMPAT_TEXTURE(u_tex, v_tex));
			    FragColor = vec4(1.0, 1.0, 1.0, alpha) * v_col;
			}
			)=====";

		auto vertexShaderAlpha = Shader::createShader(GL_VERTEX_SHADER, vertexSourceTexture);
		auto fragmentShaderAlpha = Shader::createShader(GL_FRAGMENT_SHADER, fragmentSourceAlpha);

		shaderProgramAlpha.createShaderProgram(vertexShaderAlpha, fragmentShaderAlpha);

		useProgram(nullptr);

	} // setupDefaultShaders

//////////////////////////////////////////////////////////////////////////

	static void setupQuadIndexBuffer()
	{
		std::vector<GLushort> indices(MAX_BATCH_QUADS * 6);
		for (size_t quad = 0; quad < MAX_BATCH_QUADS; ++quad)
		{
			const GLushort base = static_cast<GLushort>(quad * 4);
			GLushort* out = &indices[quad * 6];

			// Strip order (0,1,2,3) becomes two triangles with matching winding.
			out[0] = base + 0;
			out[1] = base + 1;
			out[2] = base + 2;
			out[3] = base + 2;
			out[4] = base + 1;
			out[5] = base + 3;
		}

		GLES30_CALL(glGenBuffers(1, &quadIndexBuffer));
		if (quadIndexBuffer == 0)
		{
			LOG(LogWarning) << "Unable to create the OpenGL ES quad index buffer; falling back to expanded quad batching";
			return;
		}

		GLES30_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer));

		while (glGetError() != GL_NO_ERROR)
			;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLushort), indices.data(), GL_STATIC_DRAW);
		if (glGetError() != GL_NO_ERROR)
		{
			LOG(LogWarning) << "Unable to upload the OpenGL ES quad index buffer; falling back to expanded quad batching";
			GLES30_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			GLES30_CALL(glDeleteBuffers(1, &quadIndexBuffer));
			quadIndexBuffer = 0;
			return;
		}

		GLES30_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	}

	static void setupVertexBuffer()
	{
		GLES30_CALL(glGenBuffers(1, &vertexBuffer));
		GLES30_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer));

		streamBufferGeneration = 1;
		lastStreamRange = StreamRange();
		lastLogicalVertices.clear();
		quadBatch = QuadBatch();
		performanceCounters = GLES30PerformanceCounters();

		quadBatch.vertices.reserve(MAX_BATCH_VERTICES);
		packedScratch.reserve(MAX_BATCH_QUADS * 6);

		setupQuadIndexBuffer();

	} // setupVertexBuffer

	static void setupPixelUploadBuffers()
	{
		GLuint buffers[3] = {};
		GLES30_CALL(glGenBuffers(static_cast<GLsizei>(pixelUploadSlots.size()), buffers));
		for (size_t i = 0; i < pixelUploadSlots.size(); ++i)
		{
			pixelUploadSlots[i].buffer = buffers[i];
			pixelUploadSlots[i].fence = nullptr;
			pixelUploadSlots[i].capacity = 0;
		}
	}

	static void destroyPixelUploadBuffers()
	{
		GLuint buffers[3] = {};
		GLsizei count = 0;
		for (auto& slot : pixelUploadSlots)
		{
			if (slot.fence != nullptr)
			{
				GLES30_CALL(glDeleteSync(slot.fence));
				slot.fence = nullptr;
			}
			if (slot.buffer != 0)
				buffers[count++] = slot.buffer;
			slot = PixelUploadSlot();
		}
		if (count > 0)
			GLES30_CALL(glDeleteBuffers(count, buffers));
	}

	static bool uploadTextureWithPixelBuffer(const GLenum type, const unsigned int x, const unsigned int y,
		const unsigned int width, const unsigned int height, const void* data, const size_t dataSize)
	{
		if (data == nullptr || dataSize < PIXEL_UPLOAD_THRESHOLD)
			return false;

		PixelUploadSlot* available = nullptr;
		for (auto& slot : pixelUploadSlots)
		{
			if (slot.buffer == 0)
				continue;

			if (slot.fence != nullptr)
			{
				const GLenum status = glClientWaitSync(slot.fence, 0, 0);
				if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
				{
					GLES30_CALL(glDeleteSync(slot.fence));
					slot.fence = nullptr;
				}
				else if (status == GL_WAIT_FAILED)
				{
					LOG(LogWarning) << "OpenGL ES 3.0 pixel upload fence wait failed; orphaning the slot";
					while (glGetError() != GL_NO_ERROR)
						;
					GLES30_CALL(glDeleteSync(slot.fence));
					slot.fence = nullptr;
					slot.capacity = 0;
				}
				else
					continue;
			}

			available = &slot;
			break;
		}

		// Never wait for a video upload slot. The direct path is preferable to
		// stalling the render thread when all three transfers are still in flight.
		if (available == nullptr)
			return false;

		GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, available->buffer));
		if (available->capacity < dataSize)
		{
			GLES30_CALL(glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW));
			available->capacity = dataSize;
		}

		void* mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dataSize,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		if (mapped == nullptr)
		{
			GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
			while (glGetError() != GL_NO_ERROR)
				;
			return false;
		}

		memcpy(mapped, data, dataSize);
		if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) != GL_TRUE)
		{
			GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
			return false;
		}

		GLES30_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, type, GL_UNSIGNED_BYTE, nullptr));
		available->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		if (available->fence == nullptr)
			available->capacity = 0; // Force storage orphaning before this buffer is reused.
		GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
		++performanceCounters.pboUploads;
		return true;
	}

	static GpuVertex packVertex(const Vertex& vertex)
	{
		GpuVertex packed;
		packed.x = vertex.pos.x();
		packed.y = vertex.pos.y();
		packed.u = vertex.tex.x();
		packed.v = vertex.tex.y();
		packed.col = vertex.col;
		return packed;
	}

	static StreamRange uploadPackedVertices(const GpuVertex* vertices, const size_t vertexCount)
	{
		static_assert(sizeof(GpuVertex) == 20, "GLES3 packed vertex must remain 20 bytes");

		StreamRange range;
		if (vertices == nullptr || vertexCount == 0 || vertexBuffer == 0)
			return range;

		if (vertexCount > std::numeric_limits<size_t>::max() / sizeof(GpuVertex))
			return range;

		GLES30_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer));
		GLES30_CALL(glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(GpuVertex), vertices, GL_STREAM_DRAW));

		++streamBufferGeneration;

		range.vertexCount = static_cast<GLsizei>(vertexCount);
		range.generation = streamBufferGeneration;
		range.valid = true;
		return range;
	}

	static StreamRange uploadVertices(const Vertex* vertices, const size_t vertexCount, const bool updateLogicalRange = false)
	{
		StreamRange range;
		if (vertices == nullptr || vertexCount == 0)
			return range;

		packedScratch.resize(vertexCount);
		for (size_t i = 0; i < vertexCount; ++i)
			packedScratch[i] = packVertex(vertices[i]);

		range = uploadPackedVertices(packedScratch.data(), vertexCount);

		if (range.valid && updateLogicalRange)
		{
			lastLogicalVertices.assign(vertices, vertices + vertexCount);
			lastStreamRange = range;
		}

		return range;
	}

//////////////////////////////////////////////////////////////////////////

	static GLenum convertBlendFactor(const Blend::Factor _blendFactor)
	{
		switch(_blendFactor)
		{
			case Blend::ZERO:                { return GL_ZERO;                } break;
			case Blend::ONE:                 { return GL_ONE;                 } break;
			case Blend::SRC_COLOR:           { return GL_SRC_COLOR;           } break;
			case Blend::ONE_MINUS_SRC_COLOR: { return GL_ONE_MINUS_SRC_COLOR; } break;
			case Blend::SRC_ALPHA:           { return GL_SRC_ALPHA;           } break;
			case Blend::ONE_MINUS_SRC_ALPHA: { return GL_ONE_MINUS_SRC_ALPHA; } break;
			case Blend::DST_COLOR:           { return GL_DST_COLOR;           } break;
			case Blend::ONE_MINUS_DST_COLOR: { return GL_ONE_MINUS_DST_COLOR; } break;
			case Blend::DST_ALPHA:           { return GL_DST_ALPHA;           } break;
			case Blend::ONE_MINUS_DST_ALPHA: { return GL_ONE_MINUS_DST_ALPHA; } break;
			default:                         { return GL_ZERO;                }
		}

	} // convertBlendFactor

	static void setBlendState(const bool enabled, const GLenum source = GL_ONE, const GLenum destination = GL_ZERO)
	{
		if (!stateCache.blendKnown || stateCache.blendEnabled != enabled)
		{
			GLES30_CALL(enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND));
			stateCache.blendKnown = true;
			stateCache.blendEnabled = enabled;
		}

		if (enabled && (!stateCache.blendFactorsKnown || stateCache.blendSource != source || stateCache.blendDestination != destination))
		{
			GLES30_CALL(glBlendFunc(source, destination));
			stateCache.blendFactorsKnown = true;
			stateCache.blendSource = source;
			stateCache.blendDestination = destination;
		}
	}

	static void setScissorState(const bool enabled, const Rect& rectangle = Rect())
	{
		if (enabled && (!stateCache.scissorRectKnown ||
			stateCache.scissorRect.x != rectangle.x || stateCache.scissorRect.y != rectangle.y ||
			stateCache.scissorRect.w != rectangle.w || stateCache.scissorRect.h != rectangle.h))
		{
			GLES30_CALL(glScissor(rectangle.x, rectangle.y, rectangle.w, rectangle.h));
			stateCache.scissorRect = rectangle;
			stateCache.scissorRectKnown = true;
		}

		if (!stateCache.scissorKnown || stateCache.scissorEnabled != enabled)
		{
			GLES30_CALL(enabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST));
			stateCache.scissorKnown = true;
			stateCache.scissorEnabled = enabled;
		}
	}

	static void setViewportState(const Rect& viewport)
	{
		if (stateCache.viewportKnown && stateCache.viewport.x == viewport.x && stateCache.viewport.y == viewport.y &&
			stateCache.viewport.w == viewport.w && stateCache.viewport.h == viewport.h)
			return;

		GLES30_CALL(glViewport(viewport.x, viewport.y, viewport.w, viewport.h));
		stateCache.viewport = viewport;
		stateCache.viewportKnown = true;
	}

	static void resetStateCaches()
	{
		stateCache = GLES30StateCache();
		mvpMatrixDirty = true;
		setBlendState(false);
		setScissorState(false);
	}

	static void applyBlendFactors(const Blend::Factor source, const Blend::Factor destination)
	{
		const bool blend = source != Blend::ONE && destination != Blend::ONE;
		setBlendState(blend, convertBlendFactor(source), convertBlendFactor(destination));
	}

	static void drawStreamRange(const GLenum mode, const StreamRange& range, const Blend::Factor source, const Blend::Factor destination)
	{
		if (!range.valid)
			return;

		applyBlendFactors(source, destination);
		GLES30_CALL(glDrawArrays(mode, 0, range.vertexCount));
	}

	static void flushQuadBatch()
	{
		if (!quadBatch.active || quadBatch.vertices.empty())
			return;

		if (quadBatch.shader == QuadBatchShader::COLOR_TEXTURE)
			useProgram(&shaderProgramColorTextureFast);
		else
			useProgram(&shaderProgramColorNoTexture);

		const Vertex* logical = quadBatch.vertices.data();
		const size_t quadCount = quadBatch.vertices.size() / 4;

		if (quadCount == 1)
		{
			// Nothing merged, so draw the strip rather than expanding it.
			packedScratch.resize(4);
			for (size_t i = 0; i < 4; ++i)
				packedScratch[i] = packVertex(logical[i]);

			const StreamRange range = uploadPackedVertices(packedScratch.data(), 4);
			drawStreamRange(GL_TRIANGLE_STRIP, range, quadBatch.sourceBlend, quadBatch.destinationBlend);
			++performanceCounters.singleQuadFlushes;
		}
		else if (quadIndexBuffer != 0 && quadCount <= MAX_BATCH_QUADS)
		{
			// Four vertices per quad; the static index buffer does the expansion.
			packedScratch.resize(quadCount * 4);
			for (size_t i = 0; i < quadCount * 4; ++i)
				packedScratch[i] = packVertex(logical[i]);

			const StreamRange range = uploadPackedVertices(packedScratch.data(), quadCount * 4);
			if (range.valid)
			{
				applyBlendFactors(quadBatch.sourceBlend, quadBatch.destinationBlend);
				GLES30_CALL(glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quadCount * 6),
					GL_UNSIGNED_SHORT, nullptr));
				++performanceCounters.indexedFlushes;
			}
		}
		else
		{
			packedScratch.resize(quadCount * 6);
			for (size_t quad = 0; quad < quadCount; ++quad)
			{
				const Vertex* src = logical + quad * 4;
				GpuVertex* out = &packedScratch[quad * 6];

				out[0] = packVertex(src[0]);
				out[1] = packVertex(src[1]);
				out[2] = packVertex(src[2]);
				out[3] = out[2];
				out[4] = out[1];
				out[5] = packVertex(src[3]);
			}

			const StreamRange range = uploadPackedVertices(packedScratch.data(), quadCount * 6);
			drawStreamRange(GL_TRIANGLES, range, quadBatch.sourceBlend, quadBatch.destinationBlend);
		}

		++performanceCounters.batchFlushes;
		quadBatch.vertices.clear();
		quadBatch.active = false;
	}

	static void queueQuad(const Vertex* vertices, const Blend::Factor source, const Blend::Factor destination,
		const QuadBatchShader shader)
	{
		if (quadBatch.active && (quadBatch.texture != boundTexture || quadBatch.sourceBlend != source ||
			quadBatch.destinationBlend != destination || quadBatch.shader != shader))
			flushQuadBatch();

		if (!quadBatch.active)
		{
			quadBatch.texture = boundTexture;
			quadBatch.sourceBlend = source;
			quadBatch.destinationBlend = destination;
			quadBatch.shader = shader;
			quadBatch.active = true;
		}

		quadBatch.vertices.insert(quadBatch.vertices.end(), vertices, vertices + 4);
		++performanceCounters.submittedQuads;

		// Batched quads never feed the verticesChanged == false reuse path. Clear both so
		// that path cannot pick up stale data.
		lastStreamRange.valid = false;
		lastLogicalVertices.clear();

		if (quadBatch.vertices.size() >= MAX_BATCH_VERTICES)
			flushQuadBatch();
	}

//////////////////////////////////////////////////////////////////////////

	static GLenum convertTextureType(const Texture::Type _type)
	{
		switch(_type)
		{
			case Texture::RGBA:  { return GL_RGBA; } break;
#if defined(USE_OPENGLES_30)
			case Texture::ALPHA: { return GL_RED; } break;
#elif defined(USE_OPENGLES_20)
			case Texture::ALPHA: { return GL_ALPHA; } break;
#else
			case Texture::ALPHA: { return GL_LUMINANCE_ALPHA; } break;
#endif
			default:             { return GL_ZERO; }
		}

	} // convertTextureType

	static GLenum convertTextureInternalFormat(const Texture::Type _type)
	{
#if defined(USE_OPENGLES_30)
		return _type == Texture::ALPHA ? GL_R8 : GL_RGBA8;
#else
		return convertTextureType(_type);
#endif
	}

	static bool isAlphaTexture(const GLenum _type)
	{
#if defined(USE_OPENGLES_30)
		return _type == GL_RED;
#else
		return _type == GL_ALPHA;
#endif
	}

#if defined(USE_OPENGLES_30)
	static bool isFramebufferComplete(const GLenum _target)
	{
		const GLenum status = glCheckFramebufferStatus(_target);
		if (status == GL_FRAMEBUFFER_COMPLETE)
			return true;

		LOG(LogError) << "OpenGL ES 3.0 framebuffer is incomplete, status " << status;
		return false;
	}
#endif

//////////////////////////////////////////////////////////////////////////

	#ifndef GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX
	#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
	#endif

	static int getAvailableVideoMemory()
	{
		/*
		const std::string extensions = glGetString(GL_EXTENSIONS) ? (const char*)glGetString(GL_EXTENSIONS) : "";
		if (extensions.find("GL_NVX_gpu_memory_info") != std::string::npos)
		{
			GLint totalMemoryKb = 0;
			glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalMemoryKb);
			if (totalMemoryKb != 0)
				return totalMemoryKb / 1024;
		}
		*/
		float total = 0;

		float megabytes = 4.0;
		int sz = sqrtf(megabytes * 1024.0 * 1024.0 / 4.0f);

		std::vector<unsigned int> textures;
		textures.reserve(1000);

		while (true)
		{
			unsigned int textureId = 0;
			glGenTextures(1, &textureId);

			if (textureId == 0 || glGetError() != GL_NO_ERROR)
				break;

			textures.push_back(textureId);

			bindTexture(textureId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			if (glGetError() != GL_NO_ERROR)
				break;

			total += megabytes;
		}

		while (glGetError() != GL_NO_ERROR)
			;

		for (auto tx : textures)
			glDeleteTextures(1, &tx);

		return total;
	}

//////////////////////////////////////////////////////////////////////////
	GLES30Renderer::GLES30Renderer() : mFrameBuffer(-1)
	{

	}

	unsigned int GLES30Renderer::getWindowFlags()
	{
		return SDL_WINDOW_OPENGL;

	} // getWindowFlags

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setupWindow()
	{
#if OPENGL_EXTENSIONS
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#elif defined(USE_OPENGLES_30)
		// createContext() walks back down the ladder if the driver refuses this.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ES_CONTEXT_LADDER[0].major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ES_CONTEXT_LADDER[0].minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,       8);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,           8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,         8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,          8);
		// GL_DEPTH_TEST is never enabled, so no depth attachment is requested.
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,         0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,       1);
		SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

	} // setupWindow

//////////////////////////////////////////////////////////////////////////
	std::string GLES30Renderer::getDriverName()
	{
#if OPENGL_EXTENSIONS
		return "OPENGL 3.0 / GLSL";
#elif defined(USE_OPENGLES_30)
		return "OPENGL ES 3.0";
#else
		return "OPENGL ES 2.0";
#endif
	}

	std::vector<std::pair<std::string, std::string>> GLES30Renderer::getDriverInformation()
	{
		std::vector<std::pair<std::string, std::string>> info;

		// The detected context, not getDriverName(), which is a fixed identifier used to
		// persist the Renderer setting and cannot track the version actually created.
		info.push_back(std::pair<std::string, std::string>("GRAPHICS API",
			"OPENGL ES " + std::to_string(glCapabilities.majorVersion) + "." + std::to_string(glCapabilities.minorVersion)));

		const std::string vendor = glGetString(GL_VENDOR) ? (const char*)glGetString(GL_VENDOR) : "";
		if (!vendor.empty())
			info.push_back(std::pair<std::string, std::string>("VENDOR", vendor));

		const std::string renderer = glGetString(GL_RENDERER) ? (const char*)glGetString(GL_RENDERER) : "";
		if (!renderer.empty())
			info.push_back(std::pair<std::string, std::string>("RENDERER", renderer));

		const std::string version = glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "";
		if (!version.empty())
			info.push_back(std::pair<std::string, std::string>("VERSION", version));

		const std::string shaders = glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "";
		if (!shaders.empty())
			info.push_back(std::pair<std::string, std::string>("SHADERS", shaders));

		/*
		int videoMem = getTotalMemUsage() / 1024.0 / 1024.0;
		info.push_back(std::pair<std::string, std::string>("USED VRAM", std::to_string(videoMem) + " MB"));

		videoMem = getAvailableVideoMemory();
		info.push_back(std::pair<std::string, std::string>("FREE VRAM", std::to_string(videoMem) + " MB"));
		*/
		return info;
	}

	void GLES30Renderer::createContext()
	{
		sdlContext = SDL_GL_CreateContext(getSDLWindow());

#if OPENGL_EXTENSIONS
		if (sdlContext == nullptr)
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

			sdlContext = SDL_GL_CreateContext(getSDLWindow());
			if (sdlContext == nullptr)
			{
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

				sdlContext = SDL_GL_CreateContext(getSDLWindow());
			}
		}
#elif defined(USE_OPENGLES_30)
		// setupWindow() asked for the top of the ladder; walk down until one is accepted.
		for (int i = 1; i < ES_CONTEXT_LADDER_COUNT && sdlContext == nullptr; ++i)
		{
			LOG(LogInfo) << "OpenGL ES " << ES_CONTEXT_LADDER[i - 1].major << "." << ES_CONTEXT_LADDER[i - 1].minor
				<< " context unavailable (" << SDL_GetError() << "), trying "
				<< ES_CONTEXT_LADDER[i].major << "." << ES_CONTEXT_LADDER[i].minor;

			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ES_CONTEXT_LADDER[i].major);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ES_CONTEXT_LADDER[i].minor);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

			sdlContext = SDL_GL_CreateContext(getSDLWindow());
		}
#endif

		if (sdlContext == nullptr)
		{
			const std::string error = "Unable to create " + getDriverName() + " context: " + SDL_GetError();
			LOG(LogError) << error;
			throw std::runtime_error(error);
		}

		if (SDL_GL_MakeCurrent(getSDLWindow(), sdlContext) != 0)
		{
			const std::string error = "Unable to activate " + getDriverName() + " context: " + SDL_GetError();
			LOG(LogError) << error;
			SDL_GL_DeleteContext(sdlContext);
			sdlContext = nullptr;
			throw std::runtime_error(error);
		}

#if defined(USE_OPENGLES_30)
		int contextMajor = 0;
		int contextMinor = 0;
		int contextProfile = 0;
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &contextMajor);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &contextMinor);
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &contextProfile);
		if (contextMajor < 3 || contextProfile != SDL_GL_CONTEXT_PROFILE_ES)
		{
			const std::string error = "Requested OpenGL ES 3.x but SDL created context " + std::to_string(contextMajor) + "." + std::to_string(contextMinor);
			LOG(LogError) << error;
			SDL_GL_DeleteContext(sdlContext);
			sdlContext = nullptr;
			throw std::runtime_error(error);
		}

		detectGLCapabilities();
#endif

		const std::string vendor   = glGetString(GL_VENDOR) ? (const char*)glGetString(GL_VENDOR) : "";
		const std::string renderer = glGetString(GL_RENDERER) ? (const char*)glGetString(GL_RENDERER) : "";
		const std::string version  = glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "";
		const std::string shaders  = glGetString(GL_SHADING_LANGUAGE_VERSION) ? (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) : "";
		std::string extensions;
#if defined(USE_OPENGLES_30)
		GLint extensionCount = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
		for (GLint i = 0; i < extensionCount; ++i)
		{
			const GLubyte* extension = glGetStringi(GL_EXTENSIONS, i);
			if (extension != nullptr)
			{
				if (!extensions.empty())
					extensions += " ";
				extensions += reinterpret_cast<const char*>(extension);
			}
		}
#else
		extensions = glGetString(GL_EXTENSIONS) ? (const char*)glGetString(GL_EXTENSIONS) : "";
#endif

		LOG(LogInfo) << "GL vendor:   " << vendor;
		LOG(LogInfo) << "GL renderer: " << renderer;
		LOG(LogInfo) << "GL version:  " << version;
		LOG(LogInfo) << "GL shading:  " << shaders;
		LOG(LogInfo) << "GL exts:     " << extensions;

		LOG(LogInfo) << " ARB_texture_non_power_of_two: " << (extensions.find("ARB_texture_non_power_of_two") != std::string::npos ? "ok" : "MISSING");

#if OPENGL_EXTENSIONS
		initializeGlExtensions();
#endif

		setupDefaultShaders();
		setupVertexBuffer();
		setupPixelUploadBuffers();
		resetStateCaches();

		GLES30_CALL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));

#if OPENGL_EXTENSIONS
		GLES30_CALL(glActiveTexture_(GL_TEXTURE0));
#else
		GLES30_CALL(glActiveTexture(GL_TEXTURE0));
#endif

		GLES30_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
		GLES30_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

	} // createContext

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::resetCache()
	{
		flushQuadBatch();
		useProgram(nullptr);
		bindTexture(0);
		resetStateCaches();

		for (auto customShader : _customShaderBatch)
		{
			if (customShader.second != nullptr)
			{
				customShader.second->clear();
				delete customShader.second;
			}
		}

		_customShaderBatch.clear();

		for (auto customShader : _customShaders)
		{
			if (customShader.second != nullptr)
			{
				customShader.second->deleteProgram();
				delete customShader.second;
			}
		}

		_customShaders.clear();

		if (mShaderTexture != 0)
		{
			destroyTexture(mShaderTexture);
			mShaderTexture = 0;
		}

		if (mFrameBuffer != -1)
		{
			GLES30_CALL(glDeleteFramebuffers(1, &mFrameBuffer));
			mFrameBuffer = -1;
		}
	}

	void GLES30Renderer::destroyContext()
	{
		flushQuadBatch();
		LOG(LogInfo) << "GLES3 performance counters: queued quads=" << performanceCounters.submittedQuads
			<< ", batch draws=" << performanceCounters.batchFlushes
			<< " (single-quad=" << performanceCounters.singleQuadFlushes
			<< ", indexed=" << performanceCounters.indexedFlushes << ")"
			<< ", quads per draw=" << (performanceCounters.batchFlushes > 0
				? (double)performanceCounters.submittedQuads / (double)performanceCounters.batchFlushes : 0.0)
			<< ", PBO uploads=" << performanceCounters.pboUploads
			<< ", direct large-upload fallbacks=" << performanceCounters.pboFallbacks;
		resetCache();

		shaderProgramColorTexture.deleteProgram();
		shaderProgramColorTextureFast.deleteProgram();
		shaderProgramColorNoTexture.deleteProgram();
		shaderProgramAlpha.deleteProgram();

		destroyPixelUploadBuffers();

		if (quadIndexBuffer != 0)
		{
			GLES30_CALL(glDeleteBuffers(1, &quadIndexBuffer));
			quadIndexBuffer = 0;
		}

		if (vertexBuffer != 0)
		{
			GLES30_CALL(glDeleteBuffers(1, &vertexBuffer));
			vertexBuffer = 0;
			lastStreamRange = StreamRange();
			lastLogicalVertices.clear();
			quadBatch = QuadBatch();
			packedScratch.clear();
			packedScratch.shrink_to_fit();
		}

		if (!_textures.empty())
		{
			std::vector<GLuint> textureNames;
			textureNames.reserve(_textures.size());
			for (auto& texture : _textures)
			{
				textureNames.push_back(texture.first);
				delete texture.second;
			}
			GLES30_CALL(glDeleteTextures(static_cast<GLsizei>(textureNames.size()), textureNames.data()));
			_textures.clear();
		}
		boundTexture = 0;
		boundTextureInfo = nullptr;

		SDL_GL_DeleteContext(sdlContext);
		sdlContext = nullptr;

	} // destroyContext

//////////////////////////////////////////////////////////////////////////

	unsigned int GLES30Renderer::createTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		return createTextureInternal(_type, _linear, _repeat, _width, _height, _data, false);
	}

	unsigned int GLES30Renderer::createMipmappedTexture(const Texture::Type _type, const bool _linear, const bool _repeat, const unsigned int _width, const unsigned int _height, void* _data)
	{
		return createTextureInternal(_type, _linear, _repeat, _width, _height, _data, true);
	}

	unsigned int GLES30Renderer::createTextureInternal(const Texture::Type _type, const bool _linear, const bool _repeat,
		const unsigned int _width, const unsigned int _height, void* _data, const bool _mipmapped)
	{
		flushQuadBatch();
		const GLenum type = convertTextureType(_type);
		const GLenum internalFormat = convertTextureInternalFormat(_type);

		if (_width == 0 || _height == 0)
		{
			LOG(LogError) << "CreateTexture error: texture dimensions must be non-zero";
			return 0;
		}

		unsigned int levels = 1;
		if (_mipmapped)
		{
			unsigned int dimension = std::max(_width, _height);
			while (dimension > 1)
			{
				dimension /= 2;
				++levels;
			}
		}

		unsigned int texture = 0;
		GLES30_CALL(glGenTextures(1, &texture));

		if (texture == 0)
		{
			LOG(LogError) << "CreateTexture error: glGenTextures failed ";
			return 0;
		}

		bindTexture(0);
		bindTexture(texture);

		GLES30_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
		GLES30_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, _repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
		GLES30_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
		GLES30_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _linear ? GL_LINEAR : GL_NEAREST));

		while (glGetError() != GL_NO_ERROR)
			;
		glTexStorage2D(GL_TEXTURE_2D, levels, internalFormat, _width, _height);
		GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

		if (_type == Texture::ALPHA && _data == nullptr)
		{
			const size_t texelCount = static_cast<size_t>(_width) * static_cast<size_t>(_height);
			std::vector<uint8_t> alphaData(texelCount, 0);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, type, GL_UNSIGNED_BYTE, alphaData.data());
		}
		else if (_data != nullptr)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, type, GL_UNSIGNED_BYTE, _data);

		if (_mipmapped && _data != nullptr)
			glGenerateMipmap(GL_TEXTURE_2D);

		if (glGetError() != GL_NO_ERROR)
		{
			LOG(LogError) << "CreateTexture error: immutable texture allocation or upload failed";
			bindTexture(0);
			GLES30_CALL(glDeleteTextures(1, &texture));
			return 0;
		}

		size_t memorySize = 0;
		unsigned int levelWidth = _width;
		unsigned int levelHeight = _height;
		const size_t bytesPerTexel = isAlphaTexture(type) ? 1 : 4;
		for (unsigned int level = 0; level < levels; ++level)
		{
			memorySize += static_cast<size_t>(levelWidth) * static_cast<size_t>(levelHeight) * bytesPerTexel;
			levelWidth = std::max(1u, levelWidth / 2);
			levelHeight = std::max(1u, levelHeight / 2);
		}

		auto info = new TextureInfo();
		info->type = type;
		info->size = Vector2f(_width, _height);
		info->mipLevels = levels;
		info->memorySize = memorySize;
		_textures[texture] = info;

		// Still bound from the upload above, and cached before this entry existed.
		if (boundTexture == texture)
			boundTextureInfo = info;

		return texture;

	} // createTextureInternal

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::destroyTexture(const unsigned int _texture)
	{
		if (_texture == 0)
			return;

		auto it = _textures.find(_texture);
		if (it == _textures.cend())
		{
			LOG(LogWarning) << "DestroyTexture ignored unknown or stale texture " << _texture;
			return;
		}

		flushQuadBatch();
		if (boundTexture == _texture)
		{
			GLES30_CALL(glBindTexture(GL_TEXTURE_2D, 0));
			boundTexture = 0;
			boundTextureInfo = nullptr;
		}

		delete it->second;
		_textures.erase(it);
		GLES30_CALL(glDeleteTextures(1, &_texture));

	} // destroyTexture

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::updateTexture(const unsigned int _texture, const Texture::Type _type, const unsigned int _x, const unsigned _y, const unsigned int _width, const unsigned int _height, void* _data)
	{
		const GLenum type = convertTextureType(_type);
		auto it = _textures.find(_texture);
		if (_texture == 0 || it == _textures.cend() || it->second == nullptr)
		{
			LOG(LogError) << "UpdateTexture error: unknown texture " << _texture;
			return;
		}

		TextureInfo* info = it->second;
		if (info->type != type)
		{
			LOG(LogError) << "UpdateTexture error: texture format does not match its allocation";
			return;
		}

		const unsigned int allocationWidth = static_cast<unsigned int>(info->size.x());
		const unsigned int allocationHeight = static_cast<unsigned int>(info->size.y());
		if (_x > allocationWidth || _y > allocationHeight ||
			_width > allocationWidth - _x || _height > allocationHeight - _y)
		{
			LOG(LogError) << "UpdateTexture error: update region exceeds texture allocation";
			return;
		}

		flushQuadBatch();
		bindTexture(_texture);

		bool uploaded = false;
		bool largeUploadCandidate = false;
		if (_type == Texture::RGBA && _data != nullptr && _width > 0 && _height > 0 && _x == 0 && _y == 0 &&
			_width == allocationWidth && _height == allocationHeight &&
			static_cast<size_t>(_width) <= std::numeric_limits<size_t>::max() / 4 / static_cast<size_t>(_height))
		{
			const size_t dataSize = static_cast<size_t>(_width) * static_cast<size_t>(_height) * 4;
			largeUploadCandidate = dataSize >= PIXEL_UPLOAD_THRESHOLD;
			uploaded = uploadTextureWithPixelBuffer(type, _x, _y, _width, _height, _data, dataSize);
		}

		if (!uploaded)
		{
			if (largeUploadCandidate)
				++performanceCounters.pboFallbacks;
			GLES30_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
			GLES30_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, _x, _y, _width, _height, type, GL_UNSIGNED_BYTE, _data));
		}

		if (info->mipLevels > 1)
			GLES30_CALL(glGenerateMipmap(GL_TEXTURE_2D));
		bindTexture(0);

	} // updateTexture

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::bindTexture(const unsigned int _texture)
	{
		if (boundTexture == _texture)
			return;

		flushQuadBatch();
		boundTexture = _texture;

		if(_texture == 0)
		{
			GLES30_CALL(glBindTexture(GL_TEXTURE_2D, 0));
			boundTexture = 0;
			boundTextureInfo = nullptr;
		}
		else
		{
			GLES30_CALL(glBindTexture(GL_TEXTURE_2D, _texture));
			boundTexture = _texture;

			auto it = _textures.find(_texture);
			boundTextureInfo = it != _textures.cend() ? it->second : nullptr;
		}

	} // bindTexture

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::drawLines(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		flushQuadBatch();
		const StreamRange range = uploadVertices(_vertices, _numVertices);
		useProgram(&shaderProgramColorNoTexture);
		drawStreamRange(GL_LINES, range, _srcBlendFactor, _dstBlendFactor);

	} // drawLines

//////////////////////////////////////////////////////////////////////////
	void GLES30Renderer::drawSolidRectangle(const float _x, const float _y, const float _w, const float _h, const unsigned int _fillColor, const unsigned int _borderColor, float borderWidth, float cornerRadius)
	{
		if (cornerRadius == 0.0f)
		{
			if (_fillColor != 0)
				drawRect(_x + borderWidth, _y + borderWidth, _w - borderWidth - borderWidth, _h - borderWidth - borderWidth, _fillColor);

			if (_borderColor != 0 && borderWidth > 0)
			{
				drawRect(_x, _y, _w, borderWidth, _borderColor);
				drawRect(_x + _w - borderWidth, _y + borderWidth, borderWidth, _h - borderWidth, _borderColor);
				drawRect(_x, _y + _h - borderWidth, _w - borderWidth, borderWidth, _borderColor);
				drawRect(_x, _y + borderWidth, borderWidth, _h - borderWidth - borderWidth, _borderColor);
			}
			return;
		}

		flushQuadBatch();
		bindTexture(0);
		useProgram(&shaderProgramColorNoTexture);

		auto inner = createRoundRect(_x + borderWidth, _y + borderWidth, _w - borderWidth - borderWidth, _h - borderWidth - borderWidth, cornerRadius, _fillColor);

		if ((_fillColor) & 0xFF)
		{
			const StreamRange innerRange = uploadVertices(inner.data(), inner.size());
			drawStreamRange(GL_TRIANGLE_FAN, innerRange, Blend::SRC_ALPHA, Blend::ONE_MINUS_SRC_ALPHA);
		}

		if ((_borderColor) & 0xFF && borderWidth > 0)
		{
			auto outer = createRoundRect(_x, _y, _w, _h, cornerRadius, _borderColor);

			setStencil(inner.data(), inner.size());
			GLES30_CALL(glStencilFunc(GL_NOTEQUAL, 1, ~0));

			const StreamRange outerRange = uploadVertices(outer.data(), outer.size());
			drawStreamRange(GL_TRIANGLE_FAN, outerRange, Blend::SRC_ALPHA, Blend::ONE_MINUS_SRC_ALPHA);

			disableStencil();
		}

		setBlendState(false);
	}

	void GLES30Renderer::drawTriangleStrips(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor, bool verticesChanged)
	{
		if (_vertices == nullptr || _numVertices == 0)
			return;

		bool batchableTexture = boundTexture == 0;
		QuadBatchShader batchShader = QuadBatchShader::NO_TEXTURE;
		if (boundTexture != 0)
		{
			batchableTexture = boundTextureInfo != nullptr && !isAlphaTexture(boundTextureInfo->type);
			batchShader = QuadBatchShader::COLOR_TEXTURE;
		}

		const bool batchable = verticesChanged && _numVertices == 4 && batchableTexture &&
			_vertices->customShader == nullptr && _vertices->saturation == 1.0f && _vertices->cornerRadius == 0.0f;
		if (batchable)
		{
			queueQuad(_vertices, _srcBlendFactor, _dstBlendFactor, batchShader);
			return;
		}

		flushQuadBatch();

		StreamRange range;
		if (verticesChanged)
			range = uploadVertices(_vertices, _numVertices, true);
		else if (lastStreamRange.valid && lastStreamRange.generation == streamBufferGeneration)
			range = lastStreamRange;
		else if (!lastLogicalVertices.empty())
		{
			range = uploadVertices(lastLogicalVertices.data(), lastLogicalVertices.size(), false);
			lastStreamRange = range;
		}

		if (!range.valid)
			return;

		// Setup shader
		if (boundTexture != 0)
		{
			if (boundTextureInfo != nullptr && isAlphaTexture(boundTextureInfo->type))
				useProgram(&shaderProgramAlpha);
			else
			{
				const bool hasCustomShader = _vertices->customShader != nullptr && !_vertices->customShader->path.empty();
				const bool useFastShader = !hasCustomShader && _vertices->saturation == 1.0f && _vertices->cornerRadius == 0.0f;
				ShaderProgram* shader = useFastShader ? &shaderProgramColorTextureFast : &shaderProgramColorTexture;

				if (hasCustomShader)
				{
					ShaderProgram* customShader = getShaderProgram(_vertices->customShader->path.c_str());
					if (customShader != nullptr)
						shader = customShader;
				}

				useProgram(shader);

				if (!useFastShader || hasCustomShader)
				{
					// Update uniforms used by the full built-in path and arbitrary custom shaders.
					shader->setSaturation(_vertices->saturation);
					shader->setCornerRadius(_vertices->cornerRadius);
					shader->setResolution();
					shader->setFrameCount(Renderer::getCurrentFrame());

					if (shader->supportsTextureSize() && boundTextureInfo != nullptr)
					{
						shader->setInputSize(boundTextureInfo->size);
						shader->setTextureSize(boundTextureInfo->size);
					}

					if (_numVertices > 0)
					{
						Vector2f vec = _vertices[_numVertices - 1].pos;
						if (_numVertices == 4)
						{
							vec.x() -= _vertices[0].pos.x();
							vec.y() -= _vertices[0].pos.y();
						}

						// Inverted rendering
						if (_vertices[_numVertices - 1].tex.y() == 1 && _vertices[0].tex.y() == 0)
							vec.y() = -vec.y();

						shader->setOutputSize(vec);
						shader->setOutputOffset(_vertices[0].pos);
					}

					if (hasCustomShader)
						shader->setCustomUniformsParameters(_vertices->customShader->parameters);
				}
			}
		}
		else
			useProgram(&shaderProgramColorNoTexture);

		// Reuse both the first vertex and count from the previous upload when verticesChanged is false.
		drawStreamRange(GL_TRIANGLE_STRIP, range, _srcBlendFactor, _dstBlendFactor);

	} // drawTriangleStrips

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setProjection(const Transform4x4f& _projection)
	{
		flushQuadBatch();
		projectionMatrix = _projection;
		mvpMatrix = projectionMatrix * worldViewMatrix;
		mvpMatrixDirty = true;
	} // setProjection

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setMatrix(const Transform4x4f& _matrix)
	{
		flushQuadBatch();
		worldViewMatrix = _matrix;
		// worldViewMatrix.round();
		mvpMatrix = projectionMatrix * worldViewMatrix;
		mvpMatrixDirty = true;
	} // setMatrix

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setViewport(const Rect& _viewport)
	{
		flushQuadBatch();
		// glViewport starts at the bottom left of the window.
		setViewportState(Rect(_viewport.x, getWindowHeight() - _viewport.y - _viewport.h, _viewport.w, _viewport.h));

	} // setViewport

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setScissor(const Rect& _scissor)
	{
		flushQuadBatch();
		if ((_scissor.x == 0) && (_scissor.y == 0) && (_scissor.w == 0) && (_scissor.h == 0))
			setScissorState(false);
		else
		{
			// glScissor starts at the bottom left of the window.
			setScissorState(true, Rect(_scissor.x, getWindowHeight() - _scissor.y - _scissor.h, _scissor.w, _scissor.h));
		}

	} // setScissor

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::setSwapInterval()
	{
		// vsync
		if(Settings::getInstance()->getBool("VSync"))
		{
			// SDL_GL_SetSwapInterval(0) for immediate updates (no vsync, default),
			// 1 for updates synchronized with the vertical retrace,
			// or -1 for late swap tearing.
			// SDL_GL_SetSwapInterval returns 0 on success, -1 on error.
			// if vsync is requested, try normal vsync; if that doesn't work, try late swap tearing
			// if that doesn't work, report an error
			if (SDL_GL_SetSwapInterval(1) != 0 && SDL_GL_SetSwapInterval(-1) != 0)
				LOG(LogWarning) << "Tried to enable vsync, but failed! (" << SDL_GetError() << ")";
		}
		else
			SDL_GL_SetSwapInterval(0);

	} // setSwapInterval

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::swapBuffers()
	{
		flushQuadBatch();
		useProgram(nullptr);

		GLES30_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		const GLenum discardedAttachments[] = { GL_DEPTH, GL_STENCIL };
		GLES30_CALL(glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, discardedAttachments));

#ifdef WIN32
		GLES30_CALL(glFlush());
		Sleep(0);
#endif
		SDL_GL_SwapWindow(getSDLWindow());
		GLES30_CALL(glClear(GL_COLOR_BUFFER_BIT));
	} // swapBuffers

//////////////////////////////////////////////////////////////////////////

	void GLES30Renderer::drawTriangleFan(const Vertex* _vertices, const unsigned int _numVertices, const Blend::Factor _srcBlendFactor, const Blend::Factor _dstBlendFactor)
	{
		flushQuadBatch();
		const StreamRange range = uploadVertices(_vertices, _numVertices);

		// Setup shader
		if (boundTexture != 0)
		{
			if (boundTextureInfo != nullptr && isAlphaTexture(boundTextureInfo->type))
				useProgram(&shaderProgramAlpha);
			else
			{
				if (_vertices->saturation == 1.0f)
					useProgram(&shaderProgramColorTextureFast);
				else
				{
					useProgram(&shaderProgramColorTexture);
					shaderProgramColorTexture.setSaturation(_vertices->saturation);
					shaderProgramColorTexture.setCornerRadius(0.0f);
				}
			}
		}
		else
			useProgram(&shaderProgramColorNoTexture);

		drawStreamRange(GL_TRIANGLE_FAN, range, _srcBlendFactor, _dstBlendFactor);
	}

	void GLES30Renderer::setStencil(const Vertex* _vertices, const unsigned int _numVertices)
	{
		flushQuadBatch();
		const StreamRange range = uploadVertices(_vertices, _numVertices);
		if (!range.valid)
			return;

		useProgram(&shaderProgramColorNoTexture);

		GLES30_CALL(glEnable(GL_STENCIL_TEST));
		GLES30_CALL(glClearStencil(0));
		GLES30_CALL(glClear(GL_STENCIL_BUFFER_BIT));

		GLES30_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));

		GLES30_CALL(glStencilFunc(GL_ALWAYS, 1, ~0));
		GLES30_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));

		setBlendState(true, convertBlendFactor(Blend::SRC_ALPHA), convertBlendFactor(Blend::ONE_MINUS_SRC_ALPHA));
		GLES30_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, range.vertexCount));
		setBlendState(false);

		GLES30_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

		GLES30_CALL(glStencilFunc(GL_EQUAL, 1, ~0));
		GLES30_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
	}

	void GLES30Renderer::disableStencil()
	{
		flushQuadBatch();
		GLES30_CALL(glDisable(GL_STENCIL_TEST));
	}

	size_t GLES30Renderer::getTotalMemUsage()
	{
		size_t total = 0;

		for (auto tex : _textures)
		{
			if (tex.first != 0 && tex.second)
				total += tex.second->memorySize;
		}

		return total;
	}

	bool GLES30Renderer::shaderSupportsCornerSize(const std::string& shader)
	{
		ShaderProgram* customShader = getShaderProgram(shader.c_str());
		if (customShader == nullptr)
			customShader = &shaderProgramColorTexture;

		return customShader->supportsCornerRadius();
	}

	void GLES30Renderer::postProcessShader(const std::string& path, const float _x, const float _y, const float _w, const float _h, const std::map<std::string, std::string>& parameters, unsigned int* data)
	{
		flushQuadBatch();
#if OPENGL_EXTENSIONS || defined(USE_OPENGLES_30)
#if OPENGL_EXTENSIONS
		if (glBlitFramebuffer == nullptr || glFramebufferTexture2D == nullptr)
			return;
#endif

		if (getScreenRotate() != 0 && getScreenRotate() != 2)
			return;

		ShaderBatch* shaderBatch = ShaderBatch::getShaderBatch(path.c_str());
		if (shaderBatch == nullptr || shaderBatch->size() == 0)
			return;

		if (mFrameBuffer == -1)
			GLES30_CALL(glGenFramebuffers(1, &mFrameBuffer));

		if (mFrameBuffer == 0 || mFrameBuffer == static_cast<unsigned int>(-1))
			return;

		float textureScale = 1.0f;

		// Special hack for single-pass blur shader -> Texture downscaling
		if (path == ":/shaders/blur.glsl" && data == nullptr)
		{
			auto it = parameters.find("blur");
			if (it != parameters.cend())
			{
				float blurSize = Utils::String::toFloat(it->second);

				textureScale = blurSize / 5.0f;
				if (textureScale < 1.0f)
					textureScale = 1.0f;
			}
		}


		int x = _x; int y = _y; int w = _w; int h = _h;

		if (y < 0)
		{
			h += y; y = 0;
			if (h <= 0)
				return;
		}

		if (x < 0)
		{
			w += x; x = 0;
			if (w <= 0)
				return;
		}

		int tw = w / textureScale;
		int th = h / textureScale;

		unsigned int nTextureID = 0;

		if (data != nullptr && (shaderBatch->size() - 1) % 2 == 1)
		{
			// It's the texture that will be returned into *data, so we can't cache it and we need to create a new one
			nTextureID = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
		}
		else
		{
			if (mShaderTexture == 0)
				mShaderTexture = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
			else
			{
				auto it = _textures.find(mShaderTexture);
				if (it == _textures.cend() || it->second->size.x() != tw || it->second->size.y() != th)
				{
					destroyTexture(mShaderTexture);
					mShaderTexture = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
				}
			}

			nTextureID = mShaderTexture;
		}

		if (nTextureID > 0)
		{
			int width = getScreenWidth();
			int height = getScreenHeight();

			unsigned int nFrameBuffer2 = -1;
			unsigned int nTexture2 = -1;

			if (shaderBatch->size() > 1 || data != nullptr)
			{
				// Multiple passes need another framebuffer and texture.
				GLES30_CALL(glGenFramebuffers(1, &nFrameBuffer2));
				nTexture2 = createTexture(Renderer::Texture::RGBA, true, false, tw, th, nullptr);
				if (nFrameBuffer2 == 0 || nFrameBuffer2 == static_cast<unsigned int>(-1) || nTexture2 == 0)
				{
					LOG(LogError) << "Unable to allocate OpenGL post-processing framebuffer resources";
					if (nFrameBuffer2 != 0 && nFrameBuffer2 != static_cast<unsigned int>(-1))
						GLES30_CALL(glDeleteFramebuffers(1, &nFrameBuffer2));
					if (nTexture2 != 0 && nTexture2 != static_cast<unsigned int>(-1))
						destroyTexture(nTexture2);
					if (nTextureID != mShaderTexture)
						destroyTexture(nTextureID);
					bindTexture(0);
					return;
				}
			}

			auto oldProgram = currentProgram;
			auto oldMatrix = worldViewMatrix;
			const GLES30StateCache oldState = stateCache;

			setScissorState(false);
			setBlendState(false);

			auto restoreRendererState = [&oldState]()
			{
				if (oldState.blendEnabled)
					setBlendState(true, oldState.blendSource, oldState.blendDestination);
				else
					setBlendState(false);

				if (oldState.scissorEnabled)
					setScissorState(true, oldState.scissorRect);
				else
					setScissorState(false);
			};

			setMatrix(Transform4x4f::Identity());

			Vertex vertices[4];

			if (shaderBatch->size() == 1 && data == nullptr)
			{
				vertices[0] = { { (float)x    , (float)y       }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				vertices[1] = { { (float)x    , (float)y + h   }, { 0.0f, 0.0f }, 0xFFFFFFFF };
				vertices[2] = { { (float)x + w, (float)y       }, { 1.0f, 1.0f }, 0xFFFFFFFF };
				vertices[3] = { { (float)x + w, (float)y + h   }, { 1.0f, 0.0f }, 0xFFFFFFFF };

				if (getScreenRotate() == 2)
				{
					vertices[0] = { { (float)x    , (float)y       }, { 1.0f, 0.0f }, 0xFFFFFFFF };
					vertices[1] = { { (float)x    , (float)y + h   }, { 1.0f, 1.0f }, 0xFFFFFFFF };
					vertices[2] = { { (float)x + w, (float)y       }, { 0.0f, 0.0f }, 0xFFFFFFFF };
					vertices[3] = { { (float)x + w, (float)y + h   }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				}
			}
			else
			{
				vertices[0] = { { (float)0    , (float)height - h }, { 0.0f, 1.0f }, 0xFFFFFFFF };
				vertices[1] = { { (float)0    , (float)height },     { 0.0f, 0.0f }, 0xFFFFFFFF };
				vertices[2] = { { (float)0 + w, (float)height - h }, { 1.0f, 1.0f }, 0xFFFFFFFF };
				vertices[3] = { { (float)0 + w, (float)height  },    { 1.0f, 0.0f }, 0xFFFFFFFF };
			}

			// round vertices
			for (int i = 0; i < 4; ++i)
				vertices[i].pos.round();

			StreamRange postProcessRange = uploadVertices(vertices, 4);
			if (!postProcessRange.valid)
			{
				if (nTextureID != mShaderTexture)
					destroyTexture(nTextureID);
				if (nTexture2 != static_cast<unsigned int>(-1))
					destroyTexture(nTexture2);
				if (nFrameBuffer2 != static_cast<unsigned int>(-1))
					GLES30_CALL(glDeleteFramebuffers(1, &nFrameBuffer2));
				setMatrix(oldMatrix);
				useProgram(oldProgram);
				restoreRendererState();
				return;
			}

			bool framebufferComplete = true;
			for (int i = 0; i < shaderBatch->size(); i++)
			{
				auto customShader = shaderBatch->at(i);

				if (i == 0)
				{
					bindTexture(nTextureID);

					GLES30_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
					GLES30_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFrameBuffer));
					GLES30_CALL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nTextureID, 0));
#if defined(USE_OPENGLES_30)
					if (!isFramebufferComplete(GL_DRAW_FRAMEBUFFER))
					{
						framebufferComplete = false;
						break;
					}
#endif

					const GLenum colorAttachment = GL_COLOR_ATTACHMENT0;
					GLES30_CALL(glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &colorAttachment));

					if (getScreenRotate() == 2)
						GLES30_CALL(glBlitFramebuffer(x, y, x + w, y + h, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_NEAREST));
					else
						GLES30_CALL(glBlitFramebuffer(x, height - y - h, x + w, height - y, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_NEAREST));

					if (shaderBatch->size() == 1 && data == nullptr)
						GLES30_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
					else
					{
						GLES30_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, nFrameBuffer2));
						GLES30_CALL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nTexture2, 0));
#if defined(USE_OPENGLES_30)
						if (!isFramebufferComplete(GL_DRAW_FRAMEBUFFER))
						{
							framebufferComplete = false;
							break;
						}
#endif
					}
				}
				else
				{
					bindTexture(i % 2 == 1 ? nTexture2 : nTextureID);

					if (i == shaderBatch->size() - 1 && data == nullptr)
					{
						// This is the last shader in the batch.
						vertices[0] = { { (float)x    , (float)y       }, { 0.0f, 1.0f }, 0xFFFFFFFF };
						vertices[1] = { { (float)x    , (float)y + h   }, { 0.0f, 0.0f }, 0xFFFFFFFF };
						vertices[2] = { { (float)x + w, (float)y       }, { 1.0f, 1.0f }, 0xFFFFFFFF };
						vertices[3] = { { (float)x + w, (float)y + h   }, { 1.0f, 0.0f }, 0xFFFFFFFF };

						if (getScreenRotate() == 2)
						{
							vertices[0] = { { (float)x    , (float)y       }, { 1.0f, 0.0f }, 0xFFFFFFFF };
							vertices[1] = { { (float)x    , (float)y + h   }, { 1.0f, 1.0f }, 0xFFFFFFFF };
							vertices[2] = { { (float)x + w, (float)y       }, { 0.0f, 0.0f }, 0xFFFFFFFF };
							vertices[3] = { { (float)x + w, (float)y + h   }, { 0.0f, 1.0f }, 0xFFFFFFFF };
						}

						for (int i = 0; i < 4; ++i) vertices[i].pos.round();

						postProcessRange = uploadVertices(vertices, 4);
						if (!postProcessRange.valid)
						{
							framebufferComplete = false;
							break;
						}

						GLES30_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
					}
					else
						GLES30_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, i % 2 == 1 ? mFrameBuffer : nFrameBuffer2));
				}

				useProgram(customShader);

				customShader->setSaturation(1.0f);
				customShader->setCornerRadius(0.0f);
				customShader->setTextureSize(Vector2f(tw, th));
				customShader->setInputSize(Vector2f(tw, th));
				customShader->setOutputSize(vertices[3].pos);
				customShader->setOutputOffset(vertices[0].pos);
				customShader->setResolution();
				customShader->setFrameCount(Renderer::getCurrentFrame());

				// Parameters in the glslp
				std::map<std::string, std::string> params = shaderBatch->parameters;

				// Parameters in the theme
				for (const auto& entry : parameters)
					params[entry.first] = entry.second;

				customShader->setCustomUniformsParameters(params);

				setBlendState(false);
				GLES30_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, postProcessRange.vertexCount));
			}

			if (!framebufferComplete)
			{
				GLES30_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
				bindTexture(0);
				if (nTextureID != mShaderTexture)
					destroyTexture(nTextureID);
				if (nTexture2 != static_cast<unsigned int>(-1))
					destroyTexture(nTexture2);
				useProgram(nullptr);
				setMatrix(oldMatrix);
				useProgram(oldProgram);
				restoreRendererState();
				if (nFrameBuffer2 != static_cast<unsigned int>(-1))
					GLES30_CALL(glDeleteFramebuffers(1, &nFrameBuffer2));
				return;
			}

			if (data != nullptr)
			{
				GLES30_CALL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0)); // Detach
				GLES30_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

				bool takeFirst = (shaderBatch->size() - 1) % 2 == 1;
				*data = takeFirst ? nTextureID : nTexture2;

				if (takeFirst || nTextureID != mShaderTexture)
					destroyTexture(takeFirst ? nTexture2 : nTextureID);
			}
			else
			{
				if (nTextureID != mShaderTexture)
					destroyTexture(nTextureID);

				if (nTexture2 != -1)
					destroyTexture(nTexture2);
			}

			bindTexture(0);
			useProgram(nullptr);
			setMatrix(oldMatrix);
			useProgram(oldProgram);
			restoreRendererState();

			if (nFrameBuffer2 != -1)
				GLES30_CALL(glDeleteFramebuffers(1, &nFrameBuffer2));
		}
#endif
	}

} // Renderer::

#endif // RENDERER_GLES_30
