#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <numeric>
#include <string>
#include <vector>
#include "Enclave_t.h"  // structs defined in .edl file etc
#include "sgx_trts.h"   // trusted runtime system, usually always required

#include "../defs.h"

static __inline__ int64_t rdtsc(void)
{
   unsigned int lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   return ((uint64_t)hi << 32) | lo;
}

// struct Node
// {
//    Node *next{nullptr}; // 8 Byte pointer
//    uint64_t data{0};    // 8 Byte data
//    uint64_t padding[510]; // (4096-8-8)/8=510 to create a 4KB page
// };

Node* allocateList(uint64_t num_pages)
{
   if (sizeof(Node) != PAGE_SIZE) {
      throw std::logic_error("Node size wrong" + std::to_string(sizeof(Node)));
   }
   Node* node = new Node[num_pages + 1];
   return node;
}

void freeList(Node* root)
{
   delete[] root;
}

uint64_t rnd(uint64_t& seed)
{
   uint64_t x = seed; /* state nicht mit 0 initialisieren */
   x ^= x >> 12;      // a
   x ^= x << 25;      // b
   x ^= x >> 27;      // c
   seed = x;
   return x * 0x2545F4914F6CDD1D;
}

void random_shuffle(std::vector<uint64_t>& nodes_idxs)
{
   auto n = nodes_idxs.size();
   uint64_t seed = 19650218ULL;
   for (uint64_t i = n - 1; i > 0; --i) {
      std::swap(nodes_idxs[i], nodes_idxs[rnd(seed) % (i + 1)]);
   }
}

Node* shuffleList(Node*& root, uint64_t num_pages, bool shuffle, uint64_t& counter)
{
   // -------------------------------------------------------------------------------------
   // create help vector of idx
   std::vector<uint64_t> node_idxs(num_pages - 1);  // one less because we do not start at 0 -> root should stay root
   std::iota(std::begin(node_idxs), std::end(node_idxs), 1);
   if (shuffle)
      random_shuffle(node_idxs);
   // -------------------------------------------------------------------------------------
   auto* prev = root;
   for (auto& idx : node_idxs) {
      prev->next = &root[idx];
      prev->data = counter++;
      prev = prev->next;
   }
   prev->next = nullptr;
   prev->data = counter++;
   return prev;
}

void alignPointer(Node*& node)
{
   uint64_t n = ((uintptr_t)node) % PAGE_SIZE;
   if (n != 0) {
      char* p = (char*)node;
      p = p + (PAGE_SIZE - n);
      node = (Node*)p;
   }
}

void ecall_main(uint64_t* num_pages, uint64_t* result, uint64_t* cycles, uint64_t* num_measurements)
{
   uint64_t shuffle_counter = 0;
   ocall_print_string("Allocate working list of pages");
   auto* root = allocateList(*num_pages);
   auto* root_aligned = root;
   alignPointer(root_aligned);
   Node* lastNode = shuffleList(root_aligned, *num_pages, true, shuffle_counter);
   if (((uintptr_t)root_aligned) % PAGE_SIZE != 0) {
      throw std::logic_error("Not CL aligned " + std::to_string(((uintptr_t)root_aligned) % 64));
   }
   // -------------------------------------------------------------------------------------
   ocall_print_string("Iteration starts!\n");
   auto* current = root_aligned;
   *result = 0;
   uint64_t counter = 0;
   uint64_t measurements = 0;
   std::atomic<uint64_t> cb = rdtsc();
   std::atomic<uint64_t> ce;
   while (current) {
      if (counter % OFFSET == 0 && measurements < NUM_LATENCIES) {
         ce = rdtsc();
         uint64_t duration = ce.load() - cb.load();
         cycles[measurements] = duration;
         cb.store(ce.load());
         measurements++;
      }
      *result += current->data;
      current = current->next;
      counter++;
   }
   *num_measurements = measurements;
   freeList(root);
}