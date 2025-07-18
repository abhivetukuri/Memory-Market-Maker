#include "memory_pool.hpp"
#include <cassert>
#include <iostream>

using namespace mm;

struct TestObject
{
    int value;
    char data[32];

    TestObject() : value(0) {}
    TestObject(int v) : value(v) {}
};

void test_memory_pool_basic()
{
    std::cout << "Testing basic memory pool functionality..." << std::endl;

    MemoryPool<TestObject> pool(100);

    TestObject *obj1 = pool.allocate();
    TestObject *obj2 = pool.allocate();
    TestObject *obj3 = pool.allocate();

    assert(obj1 != nullptr);
    assert(obj2 != nullptr);
    assert(obj3 != nullptr);
    assert(obj1 != obj2);
    assert(obj2 != obj3);

    obj1->value = 1;
    obj2->value = 2;
    obj3->value = 3;

    assert(obj1->value == 1);
    assert(obj2->value == 2);
    assert(obj3->value == 3);

    pool.deallocate(obj2);
    TestObject *obj4 = pool.allocate();
    assert(obj4 != nullptr);

    PoolStats stats = pool.get_stats();
    assert(stats.current_usage == 3);
    assert(stats.allocation_count == 4);
    assert(stats.free_count == 1);

    std::cout << "Basic memory pool test passed!" << std::endl;
}

void test_memory_pool_performance()
{
    std::cout << "Testing memory pool performance..." << std::endl;

    MemoryPool<TestObject> pool(1000);
    std::vector<TestObject *> objects;
    objects.reserve(1000);

    for (int i = 0; i < 1000; ++i)
    {
        TestObject *obj = pool.allocate();
        obj->value = i;
        objects.push_back(obj);
    }

    for (int i = 0; i < 500; ++i)
    {
        pool.deallocate(objects[i]);
    }

    for (int i = 0; i < 500; ++i)
    {
        TestObject *obj = pool.allocate();
        obj->value = i + 1000;
        objects.push_back(obj);
    }

    PoolStats stats = pool.get_stats();
    assert(stats.current_usage == 1000);

    std::cout << "Memory pool performance test passed!" << std::endl;
}

int main()
{
    try
    {
        test_memory_pool_basic();
        test_memory_pool_performance();
        std::cout << "All memory pool tests passed!" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}