#include "Raven/World/World.h"

namespace Raven
{
    namespace
    {
        // Advances a generation, skipping 0 so it never matches the null entity.
        std::uint32_t nextGeneration(std::uint32_t generation)
        {
            const std::uint32_t next = generation + 1;
            return next == 0 ? 1 : next;
        }
    }

    Entity World::createEntity(std::string name)
    {
        std::uint32_t index = 0;
        if (!m_freeIndices.empty())
        {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
        }
        else
        {
            index = static_cast<std::uint32_t>(m_slots.size());
            Slot fresh;
            fresh.generation = 1;
            m_slots.push_back(std::move(fresh));
        }

        Slot& slot = m_slots[index];
        slot.alive = true;
        slot.name = std::move(name);
        ++m_aliveCount;

        return Entity{index, slot.generation};
    }

    bool World::destroyEntity(Entity entity)
    {
        if (!isAlive(entity))
        {
            return false;
        }

        Slot& slot = m_slots[entity.index];
        slot.alive = false;
        slot.name.clear();
        slot.generation = nextGeneration(slot.generation);

        for (auto& storage : m_storages)
        {
            storage.second->erase(entity.index);
        }

        m_freeIndices.push_back(entity.index);
        --m_aliveCount;
        return true;
    }

    bool World::isAlive(Entity entity) const
    {
        if (entity.isNull() || entity.index >= m_slots.size())
        {
            return false;
        }
        const Slot& slot = m_slots[entity.index];
        return slot.alive && slot.generation == entity.generation;
    }

    const std::string& World::name(Entity entity) const
    {
        static const std::string empty;
        return isAlive(entity) ? m_slots[entity.index].name : empty;
    }

    void World::setName(Entity entity, std::string name)
    {
        if (isAlive(entity))
        {
            m_slots[entity.index].name = std::move(name);
        }
    }

    std::vector<Entity> World::entities() const
    {
        std::vector<Entity> result;
        result.reserve(m_aliveCount);
        for (std::uint32_t i = 0; i < m_slots.size(); ++i)
        {
            if (m_slots[i].alive)
            {
                result.push_back(Entity{i, m_slots[i].generation});
            }
        }
        return result;
    }

    void World::clear()
    {
        for (Entity entity : entities())
        {
            destroyEntity(entity);
        }
    }
}
