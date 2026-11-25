#pragma once

template <typename Key>
class ArtworkResidencyTracker
{
public:
	enum class Work
	{
		None,
		Prepare,
		Cleanup
	};

	Work workFor(const Key& key) const
	{
		if (!mValid || !(mKey == key))
			return Work::Prepare;

		return mCleanupPending ? Work::Cleanup : Work::None;
	}

	void complete(const Key& key, bool cleanupPending)
	{
		mKey = key;
		mCleanupPending = cleanupPending;
		mValid = true;
	}

	void invalidate()
	{
		mValid = false;
	}

private:
	Key mKey{};
	bool mValid = false;
	bool mCleanupPending = false;
};
