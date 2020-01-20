#pragma once

#include <windows.h>
#include <intrin.h>

#include "srw_lock.hpp"

#ifndef ASSERT
#define ASSERT(exp) ((VOID) 0)
#endif

namespace bst {

#ifndef BST_INLINE
#define BST_INLINE __forceinline
#endif

#ifdef _WIN64
#define BST_InterlockedOr(val,msk)  _InterlockedOr(val,msk)
#define BST_InterlockedBitTestAndSetPointer(ptr,val) InterlockedBitTestAndSet64((PLONGLONG)ptr, (LONGLONG)val)
#define BST_InterlockedAddPointer(ptr,val) _InterlockedAdd64((PLONGLONG)ptr,(LONGLONG)val)
#define BST_InterlockedAndPointer(ptr,val) _InterlockedAnd64((PLONGLONG)ptr,(LONGLONG)val)
#define BST_InterlockedOrPointer(ptr,val) _InterlockedOr64((PLONGLONG)ptr,(LONGLONG)val)
#define BST_InterlockedExchangePointer(ptr,val) _InterlockedExchangePointer(ptr,val)
#define BST_InterlockedCompareExchangePointer(ptr,new,val) _InterlockedCompareExchangePointer(ptr,new,val)
#else
#define BST_InterlockedOr(val,msk)  _InterlockedOr(val,msk)
#define BST_InterlockedBitTestAndSetPointer(ptr,val) InterlockedBitTestAndSet((PLONG)ptr, (LONG)val)
#define BST_InterlockedAddPointer(ptr,val) _InterlockedAdd((PLONG)ptr,(LONG)val)
#define BST_InterlockedAndPointer(ptr,val) _InterlockedAnd((PLONG)ptr,(LONG)val)
#define BST_InterlockedOrPointer(ptr,val) _InterlockedOr((PLONG)ptr,(LONG)val)
#define BST_InterlockedExchangePointer(ptr,val) _InterlockedExchange((PLONG)ptr,(LONG)val)
BST_INLINE PVOID BST_InterlockedCompareExchangePointer(PVOID volatile * Destination, PVOID Exchange, PVOID Comparand)
{
  return (PVOID)_InterlockedCompareExchange((PLONG)Destination, (LONG)Exchange, (LONG)Comparand);
}  
#endif

#define RTL_SRWLOCK_OWNED_BIT           0
#define RTL_SRWLOCK_CONTENDED_BIT       1
#define RTL_SRWLOCK_SHARED_BIT          2
#define RTL_SRWLOCK_CONTENTION_LOCK_BIT 3
#define RTL_SRWLOCK_OWNED           (1 << RTL_SRWLOCK_OWNED_BIT)
#define RTL_SRWLOCK_CONTENDED       (1 << RTL_SRWLOCK_CONTENDED_BIT)
#define RTL_SRWLOCK_SHARED          (1 << RTL_SRWLOCK_SHARED_BIT)
#define RTL_SRWLOCK_CONTENTION_LOCK (1 << RTL_SRWLOCK_CONTENTION_LOCK_BIT)
#define RTL_SRWLOCK_MASK    (RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_CONTENTION_LOCK)
#define RTL_SRWLOCK_BITS    4

typedef struct _RTLP_SRWLOCK_SHARED_WAKE
{
  LONG Wake;
  volatile struct _RTLP_SRWLOCK_SHARED_WAKE * Next;
} volatile RTLP_SRWLOCK_SHARED_WAKE, *PRTLP_SRWLOCK_SHARED_WAKE;

typedef struct _RTLP_SRWLOCK_WAITBLOCK
{
  /* SharedCount is the number of shared acquirers. */
  LONG SharedCount;

  /* Last points to the last wait block in the chain. The value is only valid when read from the first wait block. */
  volatile struct _RTLP_SRWLOCK_WAITBLOCK *Last;

  /* Next points to the next wait block in the chain. */
  volatile struct _RTLP_SRWLOCK_WAITBLOCK *Next;

  union {
    /* Wake is only valid for exclusive wait blocks */
    LONG Wake;
    /* The wake chain is only valid for shared wait blocks */
    struct {
      PRTLP_SRWLOCK_SHARED_WAKE SharedWakeChain;
      PRTLP_SRWLOCK_SHARED_WAKE LastSharedWake;
    };
  };

  BOOLEAN Exclusive;
} volatile RTLP_SRWLOCK_WAITBLOCK, *PRTLP_SRWLOCK_WAITBLOCK; 



BST_INLINE
void ReleaseWaitBlockLockExclusive(PRTL_SRWLOCK SRWLock, PRTLP_SRWLOCK_WAITBLOCK FirstWaitBlock) BST_NOEXCEPT
{
  PRTLP_SRWLOCK_WAITBLOCK Next;
  LONG_PTR NewValue;

  /* NOTE: We're currently in an exclusive lock in contended mode. */
  Next = FirstWaitBlock->Next;
  if (Next != NULL) {
    /* There's more blocks chained, we need to update the pointers in the next wait block and update the wait block pointer. */
    NewValue = (LONG_PTR)Next | RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED;
    if (!FirstWaitBlock->Exclusive) {
      /* The next wait block has to be an exclusive lock! */
      ASSERT(Next->Exclusive);
      /* Save the shared count */
      Next->SharedCount = FirstWaitBlock->SharedCount;
      NewValue |= RTL_SRWLOCK_SHARED;
    }
    Next->Last = FirstWaitBlock->Last;
  } else {
    /* Convert the lock to a simple lock. */
    if (FirstWaitBlock->Exclusive) {
      NewValue = RTL_SRWLOCK_OWNED;
    } else {
      ASSERT(FirstWaitBlock->SharedCount > 0);
      NewValue = ((LONG_PTR)FirstWaitBlock->SharedCount << RTL_SRWLOCK_BITS) | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_OWNED;
    }
  }

  BST_InterlockedExchangePointer(&SRWLock->Ptr, (PVOID)NewValue);

  if (FirstWaitBlock->Exclusive) {
    BST_InterlockedOr(&FirstWaitBlock->Wake, TRUE);
  } else {
    PRTLP_SRWLOCK_SHARED_WAKE WakeChain, NextWake;
    /* If we were the first one to acquire the shared lock, we now need to wake all others... */
    WakeChain = FirstWaitBlock->SharedWakeChain;
    do {
      NextWake = WakeChain->Next;
      BST_InterlockedOr((PLONG)&WakeChain->Wake, TRUE);
      WakeChain = NextWake;
    } while (WakeChain != NULL);
  }
}

BST_INLINE
void ReleaseWaitBlockLockLastShared(PRTL_SRWLOCK SRWLock, PRTLP_SRWLOCK_WAITBLOCK FirstWaitBlock) BST_NOEXCEPT
{
  PRTLP_SRWLOCK_WAITBLOCK Next;
  LONG_PTR NewValue;

  /* NOTE: We're currently in a shared lock in contended mode. */

  /* The next acquirer to be unwaited *must* be an exclusive lock! */
  ASSERT(FirstWaitBlock->Exclusive);

  Next = FirstWaitBlock->Next;
  if (Next != NULL) {
    /* There's more blocks chained, we need to update the pointers in the next wait block and update the wait block pointer. */
    NewValue = (LONG_PTR)Next | RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED;
    Next->Last = FirstWaitBlock->Last;
  } else {
    /* Convert the lock to a simple exclusive lock. */
    NewValue = RTL_SRWLOCK_OWNED;
  }
  BST_InterlockedExchangePointer(&SRWLock->Ptr, (PVOID)NewValue);
  BST_InterlockedOr(&FirstWaitBlock->Wake, TRUE);
}


BST_INLINE
void ReleaseWaitBlockLock(IN OUT PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  BST_InterlockedAndPointer(&SRWLock->Ptr, ~RTL_SRWLOCK_CONTENTION_LOCK);
}


BST_INLINE
PRTLP_SRWLOCK_WAITBLOCK AcquireWaitBlockLock(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  LONG_PTR PrevValue;
  PRTLP_SRWLOCK_WAITBLOCK WaitBlock;

  while (1) {
    PrevValue = BST_InterlockedOrPointer(&SRWLock->Ptr, RTL_SRWLOCK_CONTENTION_LOCK);
    if (!(PrevValue & RTL_SRWLOCK_CONTENTION_LOCK))
      break;
    YieldProcessor();
  }

  if (!(PrevValue & RTL_SRWLOCK_CONTENDED) || (PrevValue & ~RTL_SRWLOCK_MASK) == 0) {
    /* Too bad, looks like the wait block was removed in the meanwhile, unlock again */
    ReleaseWaitBlockLock(SRWLock);
    return NULL;
  }

  WaitBlock = (PRTLP_SRWLOCK_WAITBLOCK)(PrevValue & ~RTL_SRWLOCK_MASK);
  return WaitBlock;
}


BST_INLINE
void AcquireSRWLockExclusiveWait(PRTL_SRWLOCK SRWLock, PRTLP_SRWLOCK_WAITBLOCK WaitBlock)
{
  LONG_PTR CurrentValue;

  while (1) {
    CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;
    if (!(CurrentValue & RTL_SRWLOCK_SHARED)) {
      if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
        if (WaitBlock->Wake != 0) {
          /* Our wait block became the first one in the chain, we own the lock now! */
          break;
        }
      } else {
        /* The last wait block was removed and/or we're
           finally a simple exclusive lock. This means we
           don't need to wait anymore, we acquired the lock! */
        break;
      }
    }
    YieldProcessor();
  }
}


BST_INLINE
void AcquireSRWLockSharedWait(PRTL_SRWLOCK SRWLock, PRTLP_SRWLOCK_WAITBLOCK FirstWait, PRTLP_SRWLOCK_SHARED_WAKE WakeChain) BST_NOEXCEPT
{
  if (FirstWait != NULL) {
    while (WakeChain->Wake == 0) {
      YieldProcessor();
    }
  } else {
    LONG_PTR CurrentValue;
    while (1) {
      CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;
      if (CurrentValue & RTL_SRWLOCK_SHARED) {
        /* The RTL_SRWLOCK_OWNED bit always needs to be set when RTL_SRWLOCK_SHARED is set! */
        ASSERT(CurrentValue & RTL_SRWLOCK_OWNED);

        if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
          if (WakeChain->Wake != 0) {
            /* Our wait block became the first one in the chain, we own the lock now! */
            break;
          }
        } else {
          /* The last wait block was removed and/or we're
             finally a simple shared lock. This means we
             don't need to wait anymore, we acquired the lock! */
          break;
        }
      }
      YieldProcessor();
    }
  }
}


BST_INLINE
void InitializeSRWLock(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  SRWLock->Ptr = NULL;
}


BST_INLINE
void AcquireSRWLockShared(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  __declspec(align(16)) RTLP_SRWLOCK_WAITBLOCK StackWaitBlock;
  RTLP_SRWLOCK_SHARED_WAKE SharedWake;
  LONG_PTR CurrentValue, NewValue;
  PRTLP_SRWLOCK_WAITBLOCK First, Shared, FirstWait;

  while (1) {
    CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;
    if (CurrentValue & RTL_SRWLOCK_SHARED) {
      /* NOTE: It is possible that the RTL_SRWLOCK_OWNED bit is set! */
      if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
        /* There's other waiters already, lock the wait blocks and increment the shared count */
        First = AcquireWaitBlockLock(SRWLock);
        if (First != NULL) {
          FirstWait = NULL;
          if (First->Exclusive) {
            /* We need to setup a new wait block! Although
               we're currently in a shared lock and we're acquiring
               a shared lock, there are exclusive locks queued. We need
               to wait until those are released. */
            Shared = First->Last;
            if (Shared->Exclusive) {
              StackWaitBlock.Exclusive = FALSE;
              StackWaitBlock.SharedCount = 1;
              StackWaitBlock.Next = NULL;
              StackWaitBlock.Last = &StackWaitBlock;
              StackWaitBlock.SharedWakeChain = &SharedWake;

              Shared->Next = &StackWaitBlock;
              First->Last = &StackWaitBlock;

              Shared = &StackWaitBlock;
              FirstWait = &StackWaitBlock;
            } else {
              Shared->LastSharedWake->Next = &SharedWake;
              Shared->SharedCount++;
            }
          } else {
            Shared = First;
            Shared->LastSharedWake->Next = &SharedWake;
            Shared->SharedCount++;
          }

          SharedWake.Next = NULL;
          SharedWake.Wake = 0;

          Shared->LastSharedWake = &SharedWake;

          ReleaseWaitBlockLock(SRWLock);
          AcquireSRWLockSharedWait(SRWLock, FirstWait, &SharedWake);
          /* Successfully incremented the shared count, we acquired the lock */
          break;
        }
      } else {
        /* This is a fastest path, just increment the number of current shared locks */

        /* Since the RTL_SRWLOCK_SHARED bit is set, the RTL_SRWLOCK_OWNED bit also has to be set! */
        ASSERT(CurrentValue & RTL_SRWLOCK_OWNED);

        NewValue = (CurrentValue >> RTL_SRWLOCK_BITS) + 1;
        NewValue = (NewValue << RTL_SRWLOCK_BITS) | (CurrentValue & RTL_SRWLOCK_MASK);

        PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
        if ((LONG_PTR)ptr == CurrentValue) {
          /* Successfully incremented the shared count, we acquired the lock */
          break;
        }
      }
    } else {
      if (CurrentValue & RTL_SRWLOCK_OWNED) {
        /* The resource is currently acquired exclusively */
        if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
          SharedWake.Next = NULL;
          SharedWake.Wake = 0;

          /* There's other waiters already, lock the wait blocks and
             increment the shared count. If the last block in the chain
             is an exclusive lock, add another block. */
          StackWaitBlock.Exclusive = FALSE;
          StackWaitBlock.SharedCount = 0;
          StackWaitBlock.Next = NULL;
          StackWaitBlock.Last = &StackWaitBlock;
          StackWaitBlock.SharedWakeChain = &SharedWake;

          First = AcquireWaitBlockLock(SRWLock);
          if (First != NULL) {
            Shared = First->Last;
            if (Shared->Exclusive) {
              Shared->Next = &StackWaitBlock;
              First->Last = &StackWaitBlock;

              Shared = &StackWaitBlock;
              FirstWait = &StackWaitBlock;
            } else {
              FirstWait = NULL;
              Shared->LastSharedWake->Next = &SharedWake;
            }

            Shared->SharedCount++;
            Shared->LastSharedWake = &SharedWake;

            ReleaseWaitBlockLock(SRWLock);
            AcquireSRWLockSharedWait(SRWLock, FirstWait, &SharedWake);
            /* Successfully incremented the shared count, we acquired the lock */
            break;
          }
        } else {
          SharedWake.Next = NULL;
          SharedWake.Wake = 0;

          /* We need to setup the first wait block. Currently an exclusive lock is held, change the lock to contended mode. */
          StackWaitBlock.Exclusive = FALSE;
          StackWaitBlock.SharedCount = 1;
          StackWaitBlock.Next = NULL;
          StackWaitBlock.Last = &StackWaitBlock;
          StackWaitBlock.SharedWakeChain = &SharedWake;
          StackWaitBlock.LastSharedWake = &SharedWake;

          NewValue = (ULONG_PTR)&StackWaitBlock | RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED;
          PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
          if ((LONG_PTR)ptr == CurrentValue) {
            AcquireSRWLockSharedWait(SRWLock, &StackWaitBlock, &SharedWake);
            /* Successfully set the shared count, we acquired the lock */
            break;
          }
        }
      } else {
        /* This is a fast path, we can simply try to set the shared count to 1 */
        NewValue = (1 << RTL_SRWLOCK_BITS) | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_OWNED;

        /* The RTL_SRWLOCK_CONTENDED bit should never be set if neither the RTL_SRWLOCK_SHARED nor the RTL_SRWLOCK_OWNED bit is set */
        ASSERT(!(CurrentValue & RTL_SRWLOCK_CONTENDED));

        PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
        if ((LONG_PTR)ptr == CurrentValue) {
          /* Successfully set the shared count, we acquired the lock */
          break;
        }
      }
    }

    YieldProcessor();
  }
}


BST_INLINE 
void ReleaseSRWLockShared(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  LONG_PTR CurrentValue, NewValue;
  PRTLP_SRWLOCK_WAITBLOCK WaitBlock;
  BOOLEAN LastShared;

  while (1) {
    CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;
    if (CurrentValue & RTL_SRWLOCK_SHARED) {
      if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
        /* There's a wait block, we need to wake a pending exclusive acquirer if this is the last shared release */
        WaitBlock = AcquireWaitBlockLock(SRWLock);
        if (WaitBlock != NULL) {
          LastShared = (--WaitBlock->SharedCount == 0);
          if (LastShared) {
            ReleaseWaitBlockLockLastShared(SRWLock, WaitBlock);
          } else {
            ReleaseWaitBlockLock(SRWLock);
          }
          /* We released the lock */
          break;
        }
      } else {
        /* This is a fast path, we can simply decrement the shared count and store the pointer */
        NewValue = CurrentValue >> RTL_SRWLOCK_BITS;
        if (--NewValue != 0)
          NewValue = (NewValue << RTL_SRWLOCK_BITS) | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_OWNED;

        PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
        if ((LONG_PTR)ptr == CurrentValue) {
          /* Successfully released the lock */
          break;
        }
      }
    } else {
      /* The RTL_SRWLOCK_SHARED bit has to be present now, even in the contended case! */
      //RtlRaiseStatus(STATUS_RESOURCE_NOT_OWNED);
      break;
    }

    YieldProcessor();
  }
}


BST_INLINE
void AcquireSRWLockExclusive(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  __declspec(align(16)) RTLP_SRWLOCK_WAITBLOCK StackWaitBlock;
  PRTLP_SRWLOCK_WAITBLOCK First, Last;

  if (BST_InterlockedBitTestAndSetPointer(&SRWLock->Ptr, RTL_SRWLOCK_OWNED_BIT)) {
    LONG_PTR CurrentValue, NewValue;
    while (1) {
      CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;
      if (CurrentValue & RTL_SRWLOCK_SHARED) {
        /* A shared lock is being held right now. We need to add a wait block! */
        if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
          goto AddWaitBlock;
        } else {
          /* There are no wait blocks so far, we need to add ourselves as the first wait block. We need to keep the shared count! */
          StackWaitBlock.Exclusive = TRUE;
          StackWaitBlock.SharedCount = (LONG)(CurrentValue >> RTL_SRWLOCK_BITS);
          StackWaitBlock.Next = NULL;
          StackWaitBlock.Last = &StackWaitBlock;
          StackWaitBlock.Wake = 0;

          NewValue = (ULONG_PTR)&StackWaitBlock | RTL_SRWLOCK_SHARED | RTL_SRWLOCK_CONTENDED | RTL_SRWLOCK_OWNED;

          PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
          if ((LONG_PTR)ptr == CurrentValue) {
            AcquireSRWLockExclusiveWait(SRWLock, &StackWaitBlock);
            /* Successfully acquired the exclusive lock */
            break;
          }
        }
      } else {
        if (CurrentValue & RTL_SRWLOCK_OWNED) {
          /* An exclusive lock is being held right now. We need to add a wait block! */
          if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
AddWaitBlock:
            StackWaitBlock.Exclusive = TRUE;
            StackWaitBlock.SharedCount = 0;
            StackWaitBlock.Next = NULL;
            StackWaitBlock.Last = &StackWaitBlock;
            StackWaitBlock.Wake = 0;

            First = AcquireWaitBlockLock(SRWLock);
            if (First != NULL) {
              Last = First->Last;
              Last->Next = &StackWaitBlock;
              First->Last = &StackWaitBlock;

              ReleaseWaitBlockLock(SRWLock);
              AcquireSRWLockExclusiveWait(SRWLock, &StackWaitBlock);
              /* Successfully acquired the exclusive lock */
              break;
            }
          } else {
            /* There are no wait blocks so far, we need to add ourselves as the first wait block. We need to keep the shared count! */
            StackWaitBlock.Exclusive = TRUE;
            StackWaitBlock.SharedCount = 0;
            StackWaitBlock.Next = NULL;
            StackWaitBlock.Last = &StackWaitBlock;
            StackWaitBlock.Wake = 0;

            NewValue = (ULONG_PTR)&StackWaitBlock | RTL_SRWLOCK_OWNED | RTL_SRWLOCK_CONTENDED;
            PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
            if ((LONG_PTR)ptr == CurrentValue) {
              AcquireSRWLockExclusiveWait(SRWLock, &StackWaitBlock);
              /* Successfully acquired the exclusive lock */
              break;
            }
          }
        } else {
          if (!BST_InterlockedBitTestAndSetPointer(&SRWLock->Ptr, RTL_SRWLOCK_OWNED_BIT)) {
            /* We managed to get hold of a simple exclusive lock! */
            break;
          }
        }
      }

      YieldProcessor();
    }
  }
}


BST_INLINE
void ReleaseSRWLockExclusive(PRTL_SRWLOCK SRWLock) BST_NOEXCEPT
{
  LONG_PTR CurrentValue, NewValue;
  PRTLP_SRWLOCK_WAITBLOCK WaitBlock;

  while (1) {
    CurrentValue = *(volatile LONG_PTR *)&SRWLock->Ptr;

    if (!(CurrentValue & RTL_SRWLOCK_OWNED)) {
      //RtlRaiseStatus(STATUS_RESOURCE_NOT_OWNED);
      break;
    }
    if (!(CurrentValue & RTL_SRWLOCK_SHARED)) {
      if (CurrentValue & RTL_SRWLOCK_CONTENDED) {
        /* There's a wait block, we need to wake the next pending acquirer (exclusive or shared) */
        WaitBlock = AcquireWaitBlockLock(SRWLock);
        if (WaitBlock != NULL) {
          ReleaseWaitBlockLockExclusive(SRWLock, WaitBlock);
          /* We released the lock */
          break;
        }
      } else {
        /* This is a fast path, we can simply clear the RTL_SRWLOCK_OWNED
           bit. All other bits should be 0 now because this is a simple
           exclusive lock and no one is waiting. */
        ASSERT(!(CurrentValue & ~RTL_SRWLOCK_OWNED));

        NewValue = 0;
        PVOID ptr = BST_InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewValue, (PVOID)CurrentValue);
        if ((LONG_PTR)ptr == CurrentValue) {
          /* We released the lock */
          break;
        }
      }
    } else {
      /* The RTL_SRWLOCK_SHARED bit must not be present now, not even in the contended case! */
      //RtlRaiseStatus(STATUS_RESOURCE_NOT_OWNED);
      break;
    }

    YieldProcessor();
  }
}

// ================================================================================================

srw_lock::srw_lock() BST_NOEXCEPT
{
  bst::InitializeSRWLock(&m_lock);
}

srw_lock::~srw_lock() BST_NOEXCEPT
{
  unlock();
}

void srw_lock::lock_shared() BST_NOEXCEPT
{
  bst::AcquireSRWLockShared(&m_lock);
}

void srw_lock::unlock_shared() BST_NOEXCEPT
{
  bst::ReleaseSRWLockShared(&m_lock);
}

void srw_lock::lock_exclusive() BST_NOEXCEPT
{
  bst::AcquireSRWLockExclusive(&m_lock);
}

void srw_lock::unlock_exclusive() BST_NOEXCEPT
{
  bst::ReleaseSRWLockExclusive(&m_lock);
}
 
void srw_lock::unlock() BST_NOEXCEPT
{
  bst::ReleaseSRWLockExclusive(&m_lock);
  bst::ReleaseSRWLockShared(&m_lock);
}

} /* namespace */
