#pragma once

#include <functional>
#include <vector>

// Mutual-exclusion group for menus. Only one member can be active at a time.
// Controls call activate/deactivate on their group (if set). The group dismisses
// the previously active member. Controls not assigned to any group are independent.
class MenuGroup
{
public:
    void add(void* member, std::function<void()> dismiss)
    {
        entries.push_back({ member, std::move(dismiss) });
    }

    void activate(void* member)
    {
        if (active == member) return;

        if (active != nullptr)
        {
            for (auto& e : entries)
            {
                if (e.member == active)
                {
                    active = nullptr;
                    e.dismiss();
                    break;
                }
            }
        }
        active = member;
    }

    void deactivate(void* member)
    {
        if (active == member)
            active = nullptr;
    }

private:
    struct Entry
    {
        void* member;
        std::function<void()> dismiss;
    };

    std::vector<Entry> entries;
    void* active = nullptr;
};
