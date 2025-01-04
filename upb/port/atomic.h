// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef UPB_PORT_ATOMIC_H_
#define UPB_PORT_ATOMIC_H_

#include "upb/port/def.inc"

#ifdef UPB_USE_C11_ATOMICS

// IWYU pragma: begin_exports
#include <stdatomic.h>
#include <stdbool.h>
// IWYU pragma: end_exports

#define upb_Atomic_Init(addr, val) atomic_init(addr, val)
#define upb_Atomic_Load(addr, order) atomic_load_explicit(addr, order)
#define upb_Atomic_Store(addr, val, order) \
  atomic_store_explicit(addr, val, order)
#define upb_Atomic_Add(addr, val, order) \
  atomic_fetch_add_explicit(addr, val, order)
#define upb_Atomic_Sub(addr, val, order) \
  atomic_fetch_sub_explicit(addr, val, order)
#define upb_Atomic_Exchange(addr, val, order) \
  atomic_exchange_explicit(addr, val, order)
#define upb_Atomic_CompareExchangeStrong(addr, expected, desired,      \
                                         success_order, failure_order) \
  atomic_compare_exchange_strong_explicit(addr, expected, desired,     \
                                          success_order, failure_order)
#define upb_Atomic_CompareExchangeWeak(addr, expected, desired, success_order, \
                                       failure_order)                          \
  atomic_compare_exchange_weak_explicit(addr, expected, desired,               \
                                        success_order, failure_order)

#elif defined(UPB_USE_MSC_ATOMICS)
#include <intrin.h>
#include <stdbool.h>
#include <stdint.h>

#define upb_Atomic_Init(addr, val) (*(addr) = val)

#if defined(_WIN64)
// MSVC, without C11 atomics, does not have any way in pure C to force
// load-acquire store-release behavior, so we hack it with exchanges.
#pragma intrinsic(_InterlockedOr64)
#define upb_Atomic_Load(addr, order) _InterlockedOr64(addr, 0)
#pragma intrinsic(_InterlockedExchange64)
#define upb_Atomic_Store(addr, val, order) _InterlockedExchange64(addr, val)

#pragma intrinsic(_InterlockedCompareExchange64)
static bool upb_Atomic_CompareExchangeMscP(uint64_t volatile* addr,
                                           uint64_t* expected,
                                           uint64_t desired) {
  uint64_t expect_val = *expected;
  uint64_t actual_val =
      _InterlockedCompareExchange64(addr, expect_val, desired);
  if (expect_val != actual_val) {
    *expected = actual_val;
  }
  return expect_val == actual_val;
}

#pragma intrinsic(_InterlockedExchange64)
#define upb_Atomic_Exchange(addr, val, order) _InterlockedExchange64(addr, val)

#ifdef __ARM_ARCH
#pragma intrinsic(_InterlockedAdd64)
#define upb_Atomic_Add(addr, val, order) (_InterlockedAdd64((addr), (val)))
#else
#pragma intrinsic(_InterlockedExchangeAdd64)
#define upb_Atomic_Add(addr, val, order) \
  (_InterlockedExchangeAdd64(addr, val) + val)
#endif

#else  // 32 bit pointers

#pragma intrinsic(_InterlockedOr)
#define upb_Atomic_Load(addr, order) _InterlockedOr(addr, 0)
#pragma intrinsic(_InterlockedExchange)
#define upb_Atomic_Store(addr, val, order) (void)_InterlockedExchange(addr, val)
#pragma intrinsic(_InterlockedCompareExchange)
static bool upb_Atomic_CompareExchangeMscP(uint32_t volatile* addr,
                                           uint32_t* expected,
                                           uint32_t desired) {
  uint32_t expect_val = *expected;
  uint32_t actual_val = _InterlockedCompareExchange(addr, expect_val, desired);
  if (expect_val != actual_val) {
    *expected = actual_val;
  }
  return expect_val == actual_val;
}
#pragma intrinsic(_InterlockedExchange)
#define upb_Atomic_Exchange(addr, val, order) _InterlockedExchange(addr, val)

#ifdef __ARM_ARCH
#pragma intrinsic(_InterlockedAdd)
#define upb_Atomic_Add(addr, val, order) (_InterlockedAdd((addr), (val)))
#else
#pragma intrinsic(_InterlockedExchangeAdd)
#define upb_Atomic_Add(addr, val, order) \
  (_InterlockedExchangeAdd(addr, val) + val)
#endif
#define upb_Atomic_Sub(addr, val, order) upb_Atomic_Add(addr, -(val))

#endif

#define upb_Atomic_CompareExchangeStrong(addr, expected, desired,      \
                                         success_order, failure_order) \
  upb_Atomic_CompareExchangeMscP(addr, expected, desired)

#define upb_Atomic_CompareExchangeWeak(addr, expected, desired, success_order, \
                                       failure_order)                          \
  upb_Atomic_CompareExchangeMscP(addr, expected, desired)

#elif !defined(UPB_SUPPRESS_MISSING_ATOMICS)
// NOLINTNEXTLINE
#error Your compiler does not support atomic instructions, which UPB uses. If you do not use UPB on multiple threads, you can suppress this error by defining UPB_SUPPRESS_MISSING_ATOMICS.
#else  // No atomics

#include <string.h>

#define upb_Atomic_Init(addr, val) (*addr = val)
#define upb_Atomic_Load(addr, order) (*addr)
#define upb_Atomic_Store(addr, val, order) (*(addr) = val)
#define upb_Atomic_Add(addr, val, order) (*(addr) += val)
#define upb_Atomic_Sub(addr, val, order) (*(addr) -= val)

UPB_INLINE void* _upb_NonAtomic_Exchange(void* addr, void* value) {
  void* old;
  memcpy(&old, addr, sizeof(value));
  memcpy(addr, &value, sizeof(value));
  return old;
}

#define upb_Atomic_Exchange(addr, val, order) _upb_NonAtomic_Exchange(addr, val)

// `addr` and `expected` are logically double pointers.
UPB_INLINE bool _upb_NonAtomic_CompareExchangeStrongP(void* addr,
                                                      void* expected,
                                                      void* desired) {
  if (memcmp(addr, expected, sizeof(desired)) == 0) {
    memcpy(addr, &desired, sizeof(desired));
    return true;
  } else {
    memcpy(expected, addr, sizeof(desired));
    return false;
  }
}

#define upb_Atomic_CompareExchangeStrong(addr, expected, desired,      \
                                         success_order, failure_order) \
  _upb_NonAtomic_CompareExchangeStrongP((void*)addr, (void*)expected,  \
                                        (void*)desired)
#define upb_Atomic_CompareExchangeWeak(addr, expected, desired, success_order, \
                                       failure_order)                          \
  upb_Atomic_CompareExchangeStrong(addr, expected, desired, 0, 0)

#endif

#include "upb/port/undef.inc"

#endif  // UPB_PORT_ATOMIC_H_
