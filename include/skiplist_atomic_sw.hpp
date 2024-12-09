#include <vector>
#include <optional>
#include <memory>
#include <atomic>

template <typename TKey, typename TVal>
class SkipListAtomicSingleWriter
{
private:
    struct Node
    {
        Node *down;
        std::atomic<Node *> next;
        std::optional<TKey> k;
        std::atomic<TVal> v;

        Node(Node *down, Node *next, std::optional<TKey> k, TVal val) : down(down), next(next), k(k)
        {
            v.store(val, std::memory_order_relaxed);
        }

        Node(Node *down, Node *next) : down(down), next(next)
        {
        }
    };

    std::vector<Node *> heads;

    Node *findInLevel(Node *current, const TKey &k) const
    {
        Node *next = current->next.load(std::memory_order_acquire);
        while (!(next == nullptr || next->k > k))
        {
            current = next;
            next = current->next.load(std::memory_order_acquire);
        }
        return current;
    }

    Node *upsertRec(Node *current, TKey k, TVal v)
    {
        if (!current)
            return nullptr;

        Node *insertNode = findInLevel(current, k);

        // update case
        if (insertNode->k && insertNode->k == k)
        {
            insertNode->v.store(v, std::memory_order_relaxed);
            upsertRec(insertNode->down, k, v);
            return nullptr;
        }

        // we need an insert at the leaf level
        if (!insertNode->down)
        {
            auto newNode = new Node(nullptr, nullptr, k, v);
            return chainNode(insertNode, newNode);
        }

        auto childNode = upsertRec(insertNode->down, k, v);
        if (!childNode)
        {
            return nullptr;
        }

        // insert at higher level with p=0.5
        if (rand() & 1)
        {
            auto newNode = new Node(childNode, nullptr, k, v);
            return chainNode(insertNode, newNode);
        }
        return nullptr;
    }

    Node *chainNode(Node *previous, Node *newNode)
    {
        newNode->next.store(previous->next.load(), std::memory_order_relaxed);
        previous->next.store(newNode, std::memory_order_release);
        return newNode;
    }

public:
    SkipListAtomicSingleWriter(size_t height)
    {
        Node *prev = nullptr;
        for (size_t i = 0; i < height; i++)
        {
            auto newNode = new Node(nullptr, nullptr);
            if (prev)
            {
                prev->down = newNode;
            }
            prev = newNode;
            heads.push_back(newNode);
        }
    }

    ~SkipListAtomicSingleWriter()
    {
        clear();
    }

    void clear()
    {
        for (int i = 0; i < heads.size(); i++)
        {
            Node *current = heads[i];
            Node *next = heads[i]->next.load(std::memory_order_acquire);
            while (current != nullptr)
            {
                delete current;
                current = next;
                if (current == nullptr)
                    break;
                next = current->next.load(std::memory_order_acquire);
            }
        }
        heads.clear();
    }

    void upsert(TKey k, TVal v)
    {
        upsertRec(heads[0], k, v);
    }

    std::optional<TVal> find(const TKey &k) const
    {
        auto matchingNode = findInLevel(heads[0], k);
        while (matchingNode->down)
        {
            if (matchingNode->k && matchingNode->k == k)
                return matchingNode->v.load(std::memory_order_relaxed);
            matchingNode = findInLevel(matchingNode->down, k);
        }
        if (!matchingNode->k || matchingNode->k != k)
        {
            return std::nullopt;
        }
        return matchingNode->v.load(std::memory_order_relaxed);
    }

    static size_t getNodeSize()
    {
        return sizeof(Node);
    }
};
