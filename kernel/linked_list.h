#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include "debug.h"
#include "cxxutil.h"

template<typename T>
class LinkedList {
public:
    class Iterator;
private:
    class Node {
        friend LinkedList<T>;
        friend Iterator;
    private:
        T _val;
        Node* _prev;
        Node* _next;
        
        Node(const T& val, Node* prev = nullptr, Node* next = nullptr)
        : _val{val}, _prev{prev}, _next{next} {
            
        }
    public:
        T val() {
            return _val;
        }
        
        const T& val() const {
            return _val;
        }
        
        T val(const T& val) {          /* Set value, and returns old value */
            T tmp{ move(_val) };
            _val = val;
            return tmp;
        }
        
        Node* prev() {
            return _prev;
        }
        
        Node* prev() const {
            return _prev;
        }
        
        Node* next() {
            return _next;
        }
        
        Node* next() const {
            return _next;
        }
        
        void swap(Node* other) {     /* Swap value with the other object */
            ::swap(_val, other->_val);
        }
    };
public:
    class Iterator {
        friend class LinkedList<T>;
    private:
        Node* _node;
        
        Iterator(Node* node)
        : _node { node } {
            
        }
        
        void invalidate() {
            _node = nullptr;
        }
        
        Node* node() {
            return _node;
        }
        
        const Node* node() const {
            return _node;
        }
    public:
        bool end() const {
            return _node == nullptr;
        }
        
        bool has_next() const {
            return (_node != nullptr) && (_node->_next != nullptr);
        }
        
        bool valid() const {
            return !end();
        }
        
        bool next() {
            if(end())
                return false;
            _node = _node->_next;
            return true;
        }
        
        bool has_prev() const {
            return (_node != nullptr) && (_node->_prev != nullptr);
        }
        
        bool prev() {
            assert(!end());
            _node = _node->_prev;
        }
        
        const T& val() const {
            assert(!end());
            return _node->_val;
        }
        
        T val() {
            assert(!end());
            return _node->_val;
            
        }
        
        const T& operator*() const {
            assert(!end());
            return _node->_val;
        }
        
        T operator*() {
            assert(!end());
            return _node->_val;
        }
        
        const T* operator->() const {
            assert(!end());
            return &_node->_val;
        }
        
        T* operator->() {
            assert(!end());
            return &_node->_val;
        }
        
        operator const T&() const {
            assert(!end());
            return _node->val();
        }
        
        operator T() {
            assert(!end());
            return _node->val();
        }
    };
    
private:
    Node* _head;
    Node* _tail;
    
    unsigned _size;
    
    void check_node(Node* node) {
        Node* n;
        for(n = _head; n && (n != node); n = n->next());
        assert(n);
    }
    
    /* Fundamental operations */
    Node* insert_before(Node* node, const T& val) {
        Node* new_node = new Node(val);
        if(node) {
            check_node(node);
            
            new_node->_next = node;
            new_node->_prev = node->_prev;
            if(new_node->_prev)
                new_node->_prev->_next = new_node;
            node->_prev = new_node;
            
            if(node == _head)
                _head = new_node;
        } else {                /* insert before null -> insert into empty list */
            assert(!_head);
            assert(!_tail);
            _head = _tail = new_node;
        }
        
        _size++;
        return new_node;
    }
    
    Node* insert_after(Node* node, const T& val) {
        Node* new_node = new Node(val);
        if(node) {
            check_node(node);
            
            new_node->_prev = node;
            new_node->_next = node->_next;
            if(new_node->_next)
                new_node->_next->_prev = new_node;
            node->_next = new_node;
            
            if(node == _tail) {
                _tail = new_node;
            }
        } else {                            /* insert after null -> insert into empty list */
            assert(!_head);
            assert(!_tail);
            _head = _tail = new_node;
        }
        
        _size++;
        return new_node;
    }
    
    T remove(Node* node) {
        assert(node != nullptr);
        check_node(node);
        
        if(node->_prev)
            node->_prev->_next = node->_next;
        if(node->_next)
            node->_next->_prev = node->_prev;
        
        if(node == _head)
            _head = node->_next;
        if(node == _tail)
            _tail = node->_prev;
        
        T val{ move(node->_val) };
        delete node;
        
        _size--;
        return move(val);
    }
public:
    LinkedList()
    : _head(nullptr), _tail(nullptr), _size(0) {

    }
    
    ~LinkedList() {
        clear();
    }
    
    Iterator push(const T& val) {
        return(insert_before(_head, val));
    }
    
    Iterator append(const T& val) {
        return Iterator(insert_after(_tail, val));
    }
    
    void remove(Iterator i) {
        assert(i.valid());
        remove(i.node());
        i.invalidate();
    }
    
    Iterator insert_before(Iterator i, const T& val) {
        assert(i.valid());
        return Iterator(insert_before(i.node(), val));
    }
    
    void insert_after(Iterator i, const T& val) {
        assert(i.valid());
        return Iterator(insert_after(i.node(), val));
    }
    
    T pop() {
        return remove(_head);
    }

    void clear() {
        Node* n = _head;
        while (n) {
            Node* next = n->_next;
            delete n;
            n = next;
        }
        _head = _tail = nullptr;
        _size = 0;
    }
    
    bool contains(const T& val) const {
        for(Node* n = _head; n; n = n->next()) {
            if(n->val() == val)
                return true;
        }
        return false;
    }
    
    /* Get node at specified index, or throws error if invalid index */
    Iterator get_iterator_at(unsigned index) const {
        unsigned current_index = 0;
        for(Iterator i = iterator(); !i.end(); i.next()) {
            if(current_index == index)
                return i;
            current_index++;
        }
        assert(false);
    }
    
    /* Throws error if specified index does not exists */
    T get(unsigned index) const {
        Iterator i = get_iterator_at(index);
        assert(!i.end());
        return i.val();
    }
    
    /* Returns -1 if specified value does not exists in list */
    int index_of(const T& val) const {
        unsigned idx = 0;
        for(Iterator i = iterator(); !i.end(); i.next()) {
            if(i.val() == val) {
                return idx;
            }
            idx++;
        }
        return -1;
    }
    
    /* Returns -1 if specified value does not exists in list */
    int last_index_of(const T& val) const {
        unsigned idx = size() - 1;
        for(Iterator i = reverse_iterator(); !i.end(); i.prev()) {
            if(i.val() == val)
                return idx;
            idx--;
        }
        return -1;
    }
    
    /* Finds first occurence of specified value, or returns an iterator pointing to tail */
    Iterator find(const T& val) {
        Iterator i = iterator();
        for(; !i.end(); i.next()) {
            if(i.val() == val)
                return i;
        }
        return i;
    }
    
    /* Finds first occurence of specified value, or returns an iterator pointing to tail */
    Iterator find(const T& val) const {
        Iterator i = iterator();
        for(; !i.end(); i.next()) {
            if(i.val() == val)
                return i;
        }
        return i;
    }
    
    /* Removes first instance of specified value from this list, or do nothing if value not found */
    void remove_val(const T& val) {
        for(Iterator i = iterator(); !i.end(); i.next()) {
            if(i.val() == val) {
                remove(i);
                return;
            }
        }
    }
    
    /* Removes all occurences of specified value from list, or do nothing if value not found */
    void remove_all(const T& val) {
        while (true) {
            Iterator i = find(val);
            if(i.end())
                return;
            remove(i);
        }
    }
    
    /* Set value of element at specified index, or throws error if specified index is invalid */
    void set(unsigned index, const T& val) {
        Iterator i = get_iterator_at(index);
        i.val(val);
    }
    
    /* Insert element at specified index (i.e. after current element at index), or throws error if
     specified index is invalid */
    void insert(unsigned index, const T& val) {
        Iterator i = get_iterator_at(index);
        insert_after(i, val);
    }
    
    /* Get size */
    unsigned size() const {
        return _size;
    }
    
    typedef int (*comparator_t)(const T&, const T&);    /* Element comparator */
    
    /* bubble sort */
    void sort(comparator_t comparator) {
        if(size() <= 1)
            return;
        
        /* Simple bubble sort */
        while (true) {
            bool changed = false;
            for(Node* n = _head ; n && n->next(); n = n->next()) {
                int compare_result = comparator(n->val(), n->next()->val());
                if(compare_result < 0) {
                    /* swap */
                    n->swap(n->next());
                    changed = true;
                }
            }
            if(!changed)
                return;
        }
    }
    
    /* Returns an iterator pointing to head */
    Iterator iterator() {
        return Iterator(_head);
    }
    
    /* Returns an iterator pointing to tail */
    Iterator reverse_iterator() {
        return Iterator(_tail);
    }
};

#endif //_LINKED_LIST_H_
