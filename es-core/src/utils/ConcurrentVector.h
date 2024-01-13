#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

template <typename T>
class ConcurrentVector 
{
public:
    void clear();

    void push_back(const T& value);
    size_t size() const;
    std::vector<T> toVector() const;

    bool contains(const T& value) const;

private:
    std::vector<T> vector_;
    mutable std::shared_timed_mutex mutex_;
};

template <typename T>
void ConcurrentVector<T>::clear()
{
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    vector_.clear();
}

template <typename T>
void ConcurrentVector<T>::push_back(const T& value)
{
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    vector_.push_back(value);
}

template <typename T>
std::vector<T> ConcurrentVector<T>::toVector() const
{
    std::shared_lock<std::shared_timed_mutex> lock(mutex_);
    return vector_;
}

template <typename T>
size_t ConcurrentVector<T>::size() const
{
    std::shared_lock< std::shared_timed_mutex> lock(mutex_);
    return vector_.size();
}

template <typename T>
bool ConcurrentVector<T>::contains(const T& value) const
{
    std::shared_lock< std::shared_timed_mutex> lock(mutex_);
    return std::find(vector_.begin(), vector_.end(), value) != vector_.end();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename Key, typename Value>
class ConcurrentMap {
public:
    void clear()
    {
        std::unique_lock< std::shared_timed_mutex> lock(mutex_);
        map_.clear();
    }

    void insert(const Key& key, const Value& value) 
    {
        std::unique_lock< std::shared_timed_mutex> lock(mutex_);
        map_[key] = value;
    }

    bool find(const Key& key, Value& value) {
        std::shared_lock< std::shared_timed_mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

private:
    std::unordered_map<Key, Value> map_;
    mutable std::shared_timed_mutex mutex_;
};