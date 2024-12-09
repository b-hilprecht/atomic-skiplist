#include <vector>
#include <list>
#include <optional>
#include <memory>

template <typename TKey, typename TVal>
class SkipList
{
private:
    struct Node
    {
        Node *down;
        std::unique_ptr<Node> next;
        std::optional<TKey> k;
        std::optional<TVal> v;

        Node(Node *down, std::unique_ptr<Node> next, std::optional<TKey> k, std::optional<TVal> v) : down(down), next(std::move(next)), k(k), v(v) {}
    };

    std::vector<std::unique_ptr<Node>> heads;

    Node *findInLevel(Node *current, const TKey &k) const
    {
        Node *next = current->next.get();
        while (!(next == nullptr || next->k > k))
        {
            current = next;
            next = current->next.get();
        }
        return current;
    }

    Node *upsertRec(Node *current, TKey k, TVal v)
    {
        if (!current)
        {
            return nullptr;
        }
        Node *insertNode = findInLevel(current, k);

        // update case
        if (insertNode->k && insertNode->k == k)
        {
            insertNode->v = v;
            upsertRec(insertNode->down, k, v);
            return nullptr;
        }

        // we need an insert at the leaf level
        if (!insertNode->down)
        {
            auto newNode = std::make_unique<Node>(nullptr, nullptr, k, v);
            return chainNode(insertNode, std::move(newNode));
        }

        auto childNode = upsertRec(insertNode->down, k, v);
        if (!childNode)
        {
            return nullptr;
        }

        // insert at higher level with p=0.5
        if (rand() & 1)
        {
            auto newNode = std::make_unique<Node>(childNode, nullptr, k, v);
            return chainNode(insertNode, std::move(newNode));
        }
        return nullptr;
    }

    Node *chainNode(Node *previous, std::unique_ptr<Node> &&newNode)
    {
        Node *return_ptr = newNode.get();
        newNode->next = std::move(previous->next);
        previous->next = std::move(newNode);
        return return_ptr;
    }

public:
    SkipList(size_t height)
    {
        Node *prev = nullptr;
        for (size_t i = 0; i < height; i++)
        {
            auto newNode = std::make_unique<Node>(nullptr, nullptr, std::nullopt, std::nullopt);
            if (prev)
            {
                prev->down = newNode.get();
            }
            prev = newNode.get();
            heads.push_back(std::move(newNode));
        }
    }

    ~SkipList()
    {
        clear();
    }

    void clear()
    {
        for (int i = 0; i < heads.size(); i++)
        {
            auto clearPointer = std::move(heads[i]->next);
            while (clearPointer)
            {
                clearPointer = std::move(clearPointer->next);
            }
        }
        heads.clear();
    }

    void upsert(TKey k, TVal v)
    {
        upsertRec(heads[0].get(), k, v);
    }

    std::optional<TVal> find(const TKey &k) const
    {
        auto matchingNode = findInLevel(heads[0].get(), k);
        while (matchingNode->down)
        {
            if (matchingNode->k && matchingNode->k == k)
                return matchingNode->v;
            matchingNode = findInLevel(matchingNode->down, k);
        }
        if (!matchingNode->k || matchingNode->k != k)
        {
            return std::nullopt;
        }
        return matchingNode->v;
    }

    static size_t getNodeSize()
    {
        return sizeof(Node);
    }
};
