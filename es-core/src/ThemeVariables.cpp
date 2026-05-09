#include "ThemeVariables.h"

#include <cstring>

std::string ThemeVariables::resolvePlaceholders(const char* in) const
{
    if (in == nullptr || in[0] == 0) 
        return "";

    std::string result;
    result.reserve(strlen(in));

    std::string_view view(in);
    size_t pos = 0;

    while (true)
    {
        size_t start = view.find("${", pos);
        if (start == std::string_view::npos) 
        {
            result.append(view.substr(pos));
            break;
        }

        result.append(view.substr(pos, start - pos));

        size_t end = view.find('}', start + 2);
        if (end == std::string_view::npos) 
        {
            result.append(view.substr(start));
            break;
        }

        std::string_view varName = view.substr(start + 2, end - (start + 2));

        auto it = this->find(varName);
        if (it != this->cend())
            result.append(it->second);        

        pos = end + 1;
    }

    return result;    
}
