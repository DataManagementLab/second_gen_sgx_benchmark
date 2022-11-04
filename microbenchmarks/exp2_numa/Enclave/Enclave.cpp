#include <assert.h>
#include <stdlib.h>
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

struct Node {
   Node* next{nullptr};
   uint64_t data{0};     // just to ensure that it will not be outcompiled
   uint64_t padding[6];  // (64 -8-8)/8 (8 is lengtho of uint64)
};

Node* allocateList(uint64_t number_nodes)
{
   if (sizeof(Node) != 64) {
      throw std::logic_error("Node size wrong" + std::to_string(sizeof(Node)));
   }
   Node* node = new Node[number_nodes + 1];
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

void shuffleList(Node*& root, uint64_t number_nodes, bool shuffle)
{
   // -------------------------------------------------------------------------------------
   // create help vector of idx
   std::vector<uint64_t> node_idxs(number_nodes - 1);  // one less because we do not start at 0 -> root should stay root
   std::iota(std::begin(node_idxs), std::end(node_idxs), 1);
   if (shuffle)
      random_shuffle(node_idxs);
   // -------------------------------------------------------------------------------------
   uint64_t counter = 0;
   auto* prev = root;
   for (auto& idx : node_idxs) {
      prev->next = &root[idx];
      prev->data = counter++;
      prev = prev->next;
   }
}

void ecall_main(uint64_t* number_nodes,
                uint64_t* prealloc,
                uint64_t* result,
                uint64_t* cycles,
                uint64_t* num_measurements,
                uint8_t shuffle)
{
   // -------------------------------------------------------------------------------------
   ocall_print_string("Preallocating");
   auto palloc = new char[*prealloc];
   ocall_print_string("allocating List");
   auto root = allocateList(*number_nodes);
   auto node = root;

   uint64_t n = ((uintptr_t)node) % 64;
   if (n != 0) {
      char* p = (char*)node;
      p = p + (64 - n);
      node = (Node*)p;
   }
   if (((uintptr_t)node) % 64 != 0) {
      throw std::logic_error("Not CL aligned " + std::to_string(((uintptr_t)node) % 64));
   }
   *result = 0;
   shuffleList(node, *number_nodes, shuffle);
   // iterate
   ocall_print_string("Iteration starts!\n");
   auto* current = node;
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
   delete palloc;
}
