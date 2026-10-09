#pragma once

#include "Raven/World/Entity.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Raven
{
    namespace detail
    {
        struct IStorage
        {
            virtual ~IStorage() = default;
            virtual void erase(std::uint32_t index) = 0;
        };

        template <typename T>
        struct Storage final : IStorage
        {
            std::unordered_map<std::uint32_t, T> items;

            void erase(std::uint32_t index) override { items.erase(index); }
        };
    }

    // Owns entities and their components. Components are stored per type,
    // keyed by entity index. Destroying an entity removes all its components.
    class World
    {
    public:
        World() = default;
        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // Entity lifecycle

        Entity createEntity(std::string name = {});
        bool destroyEntity(Entity entity);
        bool isAlive(Entity entity) const;

        const std::string& name(Entity entity) const;
        void setName(Entity entity, std::string name);

        std::size_t entityCount() const { return m_aliveCount; }

        // Live entities in slot-index order.
        std::vector<Entity> entities() const;

        // Destroys every entity. Existing handles become invalid.
        void clear();

        // Components

        // Adds or replaces T on the entity. Returns nullptr if the entity is not alive.
        template <typename T, typename... Args>
        T* add(Entity entity, Args&&... args)
        {
            if (!isAlive(entity))
            {
                return nullptr;
            }
            auto& items = storage<T>().items;
            auto result = items.insert_or_assign(entity.index, T{std::forward<Args>(args)...});
            return &result.first->second;
        }

        template <typename T>
        T* get(Entity entity)
        {
            return const_cast<T*>(static_cast<const World*>(this)->get<T>(entity));
        }

        template <typename T>
        const T* get(Entity entity) const
        {
            if (!isAlive(entity))
            {
                return nullptr;
            }
            const auto* s = findStorage<T>();
            if (!s)
            {
                return nullptr;
            }
            auto it = s->items.find(entity.index);
            return it == s->items.end() ? nullptr : &it->second;
        }

        template <typename T>
        bool has(Entity entity) const
        {
            return get<T>(entity) != nullptr;
        }

        template <typename T>
        bool remove(Entity entity)
        {
            if (!isAlive(entity))
            {
                return false;
            }
            auto* s = findStorage<T>();
            return s && s->items.erase(entity.index) > 0;
        }

        // All live entities that have component T.
        template <typename T>
        std::vector<Entity> entitiesWith() const
        {
            std::vector<Entity> result;
            for (Entity entity : entities())
            {
                if (has<T>(entity))
                {
                    result.push_back(entity);
                }
            }
            return result;
        }

    private:
        struct Slot
        {
            std::uint32_t generation = 0;
            bool alive = false;
            std::string name;
        };

        template <typename T>
        detail::Storage<T>& storage()
        {
            const std::type_index key(typeid(T));
            auto it = m_storages.find(key);
            if (it == m_storages.end())
            {
                it = m_storages.emplace(key, std::make_unique<detail::Storage<T>>()).first;
            }
            return *static_cast<detail::Storage<T>*>(it->second.get());
        }

        template <typename T>
        detail::Storage<T>* findStorage() const
        {
            auto it = m_storages.find(std::type_index(typeid(T)));
            return it == m_storages.end()
                ? nullptr
                : static_cast<detail::Storage<T>*>(it->second.get());
        }

        std::vector<Slot> m_slots;
        std::vector<std::uint32_t> m_freeIndices;
        std::size_t m_aliveCount = 0;
        std::unordered_map<std::type_index, std::unique_ptr<detail::IStorage>> m_storages;
    };
}
