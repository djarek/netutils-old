#ifndef NETU_COMPLETION_HANDLER_LIST_HPP
#define NETU_COMPLETION_HANDLER_LIST_HPP

#include <netu/detail/handler_erasure.hpp>

#include <boost/intrusive/list.hpp>

namespace netu
{

template<typename Signature>
class completion_handler_list;

template<typename Node>
class unique_node
{
public:
    template<typename... Ts>
    void invoke(Ts&&... ts)
    {
        auto* p = ptr_.release();
        // TODO(djarek): What if moving/copying arguments throws?
        return p->invoke(std::forward<Ts>(ts)...);
    }

    void reset()
    {
        ptr_.reset();
    }

    friend bool operator==(unique_node const& lhs, std::nullptr_t)
    {
        return lhs.ptr_ == nullptr;
    }

    friend bool operator==(std::nullptr_t, unique_node const& rhs)
    {
        return nullptr == rhs.ptr_;
    }

    friend bool operator!=(unique_node const& lhs, std::nullptr_t)
    {
        return lhs.ptr_ != nullptr;
    }

    friend bool operator!=(std::nullptr_t, unique_node const& rhs)
    {
        return nullptr != rhs.ptr_;
    }

    explicit operator bool() const noexcept
    {
        return *this != nullptr;
    }

private:
    explicit unique_node(Node* n) noexcept
      : ptr_{n}
    {
    }

    template<typename Signature>
    friend class completion_handler_list;

    std::unique_ptr<Node, detail::node_deleter> ptr_;
};

template<typename R, typename... Ts>
class completion_handler_list<R(Ts...)>
{
    class node_base;

    template<typename CompletionHandler>
    class handler_node;

public:
    using value_type = node_base;
    using iterator = typename boost::intrusive::list<value_type>::iterator;
    using const_iterator =
      typename boost::intrusive::list<value_type>::const_iterator;
    using released_node = unique_node<node_base>;

    completion_handler_list() = default;

    completion_handler_list(completion_handler_list&&) noexcept = default;
    completion_handler_list& operator=(completion_handler_list&&) noexcept =
      default;

    completion_handler_list(completion_handler_list const&) = delete;
    completion_handler_list& operator=(completion_handler_list const&) = delete;

    ~completion_handler_list()
    {
        clear();
    }

    iterator begin()
    {
        return handlers_.begin();
    }

    iterator end()
    {
        return handlers_.end();
    }

    const_iterator begin() const
    {
        return handlers_.begin();
    }

    const_iterator end() const
    {
        return handlers_.end();
    }

    const_iterator cbegin() const
    {
        return handlers_.cbegin();
    }

    const_iterator cend() const
    {
        return handlers_.cend();
    }

    value_type& front()
    {
        return handlers_.front();
    }

    value_type const& front() const
    {
        return handlers_.front();
    }

    value_type& back()
    {
        return handlers_.back();
    }

    value_type const& back() const
    {
        return handlers_.back();
    }

    bool empty() const noexcept
    {
        return handlers_.empty();
    }

    std::size_t size() const noexcept
    {
        return handlers_.size();
    }

    void clear() noexcept
    {
        handlers_.clear_and_dispose(detail::node_deleter{});
    }

    template<typename CompletionHandler>
    iterator insert(iterator pos, CompletionHandler&& h)
    {
        using handler_node_t =
          handler_node<typename std::remove_reference<CompletionHandler>::type>;
        auto p =
          detail::allocate_handler_node<R(Ts...), handler_node_t, node_base>(
            std::forward<CompletionHandler>(h));
        auto it = handlers_.insert(pos, *p);
        p.release();
        return it;
    }

    iterator erase(iterator pos) noexcept
    {
        return handlers_.erase_and_dispose(pos, detail::node_deleter{});
    }

    iterator erase(iterator first, iterator last) noexcept
    {
        return handlers_.erase_and_dispose(first, last, detail::node_deleter{});
    }

    template<typename... Args>
    iterator erase_invoke(iterator pos, Args&&... args)
    {
        auto pair = erase_release(pos);
        pair.second.invoke(std::forward<Args>(args)...);
        return pair.first;
    }

    std::pair<iterator, released_node> erase_release(iterator pos) noexcept
    {
        auto& h = *pos;
        auto it = handlers_.erase(pos);
        return {it, released_node{&h}};
    }

    template<typename CompletionHandler>
    void push_front(CompletionHandler&& h)
    {
        insert(handlers_.begin(), std::forward<CompletionHandler>(h));
    }

    released_node pop_front()
    {
        BOOST_ASSERT(!handlers_.empty());
        auto pair = erase_release(handlers_.begin());
        return std::move(pair.second);
    }

    template<typename CompletionHandler>
    void push_back(CompletionHandler&& h)
    {
        insert(handlers_.end(), std::forward<CompletionHandler>(h));
    }

    released_node pop_back()
    {
        BOOST_ASSERT(!handlers_.empty());
        auto pair = erase_release(std::prev(handlers_.end()));
        return std::move(pair.second);
    }

    void swap(completion_handler_list& other) noexcept
    {
        return handlers_.swap(other.handlers_);
    }

    friend void swap(completion_handler_list& lhs,
                     completion_handler_list& rhs) noexcept
    {
        return lhs.swap(rhs);
    }

    void splice(const_iterator pos, completion_handler_list& other) noexcept
    {
        handlers_.splice(pos, other.handlers_);
    }

    void splice(const_iterator pos, completion_handler_list&& other) noexcept
    {
        splice(pos, other);
    }

    void splice(const_iterator pos,
                completion_handler_list& other,
                const_iterator new_element) noexcept
    {
        handlers_.splice(pos, other.handlers_, new_element);
    }

    void splice(const_iterator pos,
                completion_handler_list&& other,
                const_iterator new_element) noexcept
    {
        splice(pos, other, new_element);
    }

    void splice(const_iterator pos,
                completion_handler_list& other,
                const_iterator first,
                const_iterator last) noexcept
    {
        handlers_.splice(pos, other.handlers_, first, last);
    }

    void splice(const_iterator pos,
                completion_handler_list&& other,
                const_iterator first,
                const_iterator last) noexcept
    {
        splice(pos, other, first, last);
    }

private:
    boost::intrusive::list<value_type> handlers_;
};

template<typename R, typename... Ts>
class completion_handler_list<R(Ts...)>::node_base
  : public boost::intrusive::list_base_hook<>
{
public:
    explicit node_base(detail::node_vtable<R(Ts...)> const* vtbl)
      : vtbl_{vtbl}
    {
    }

    node_base& operator=(node_base&&) = delete;
    node_base(node_base const&) = delete;
    node_base& operator=(node_base const&) = delete;

    template<typename... Us>
    R invoke(Us&&... us)
    {
        BOOST_ASSERT(!this->is_linked());
        vtbl_->invoke(this, std::forward<Us>(us)...);
    }

    void destroy()
    {
        BOOST_ASSERT(!this->is_linked());
        vtbl_->destroy(this);
    }

protected:
    node_base(node_base&&) = default;
    ~node_base() = default;

private:
    detail::node_vtable<R(Ts...)> const* const vtbl_;
};

template<typename R, typename... Ts>
template<typename CompletionHandler>
class completion_handler_list<R(Ts...)>::handler_node
  : public completion_handler_list<R(Ts...)>::node_base
{
public:
    using allocator_type =
      detail::allocators::rebound_alloc_t<CompletionHandler, handler_node>;

    template<typename T>
    explicit handler_node(detail::node_vtable<R(Ts...)> const* vtbl, T&& t)
      : node_base{vtbl}
      , handler_{std::forward<T>(t)}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return detail::allocators::rebind_associated<handler_node>(handler_);
    }

    template<typename... Us>
    R operator()(Us&&... us)
    {
        return handler_(std::forward<Us>(us)...);
    }

private:
    CompletionHandler handler_;
};

} // namespace netu

#endif // NETU_COMPLETION_HANDLER_LIST_HPP
