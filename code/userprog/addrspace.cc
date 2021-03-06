// addrspace.cc 
//      Routines to manage address spaces (executing user programs).
//
//      In order to run a user program, you must:
//
//      1. link with the -N -T 0 option 
//      2. run coff2noff to convert the object file to Nachos format
//              (Nachos object code format is essentially just a simpler
//              version of the UNIX executable object code format)
//      3. load the NOFF file into the Nachos file system
//              (if you haven't implemented the file system yet, you
//              don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include <strings.h>		/* for bzero */
#ifdef CHANGED
#include "synch.h"

static void ReadAtVirtual( OpenFile *executable, int virtualaddr,
    int numBytes, int position, TranslationEntry *pT, unsigned numPages);
#endif //CHANGED

//----------------------------------------------------------------------
// SwapHeader
//      Do little endian to big endian conversion on the bytes in the 
//      object file header, in case the file was generated on a little
//      endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader * noffH)
{
    noffH->noffMagic = WordToHost (noffH->noffMagic);
    noffH->code.size = WordToHost (noffH->code.size);
    noffH->code.virtualAddr = WordToHost (noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost (noffH->code.inFileAddr);
    noffH->initData.size = WordToHost (noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost (noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost (noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost (noffH->uninitData.size);
    noffH->uninitData.virtualAddr =
	WordToHost (noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost (noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
//      Create an address space to run a user program.
//      Load the program from a file "executable", and set everything
//      up so that we can start executing user instructions.
//
//      Assumes that the object code file is in NOFF format.
//
//      First, set up the translation from program memory to physical 
//      memory.  For now, this is really simple (1:1), since we are
//      only uniprogramming, and we have a single unsegmented page table
//
//      "executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace (OpenFile * executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt ((char *) &noffH, sizeof (noffH), 0);

    if ((noffH.noffMagic != NOFFMAGIC) &&
	(WordToHost (noffH.noffMagic) == NOFFMAGIC))
	SwapHeader (&noffH);
    ASSERT (noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	// we need to increase the size
    // to leave room for the stack
    numPages = divRoundUp (size, PageSize);
    size = numPages * PageSize;

    ASSERT (numPages <= NumPhysPages);	// check we're not trying
    // to run anything too big --
    // at least until we have
    // virtual memory

    DEBUG ('a', "Initializing address space, num pages %d, size %d\n",
	   numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++)
      {
	  pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
    #ifdef CHANGED
	  pageTable[i].physicalPage = machine->myFrameProvider->GetEmptyFrame();//i+1;
    #endif
	  pageTable[i].valid = TRUE;
	  pageTable[i].use = FALSE;
	  pageTable[i].dirty = FALSE;
	  pageTable[i].readOnly = FALSE;	// if the code segment was entirely on 
	  // a separate page, we could set its 
	  // pages to be read-only
      }

// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    //bzero (machine->mainMemory, size);

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0)
      {
	  DEBUG ('a', "Initializing code segment, at 0x%x, size %d\n",
		 noffH.code.virtualAddr, noffH.code.size);
	  //executable->ReadAt (&(machine->mainMemory[noffH.code.virtualAddr]),
		//	      noffH.code.size, noffH.code.inFileAddr);
    #ifdef CHANGED
    ReadAtVirtual(executable, noffH.code.virtualAddr,
      noffH.code.size, noffH.code.inFileAddr, pageTable, numPages);
    #endif
      }

    if (noffH.initData.size > 0)
      {
	  DEBUG ('a', "Initializing data segment, at 0x%x, size %d\n",
		 noffH.initData.virtualAddr, noffH.initData.size);
	  //executable->ReadAt (&
		//	      (machine->mainMemory
		//	       [noffH.initData.virtualAddr]),
		//	      noffH.initData.size, noffH.initData.inFileAddr);
    #ifdef CHANGED
    ReadAtVirtual(executable, noffH.initData.virtualAddr,
      noffH.initData.size, noffH.initData.inFileAddr, pageTable, numPages);
    #endif
      }

      #ifdef CHANGED
      int lengthBitMap = (int)(UserStackSize/(PagePerThread*PageSize));
      int j;
      bitmapThreadStack = new BitMap(lengthBitMap);
      bitmapThreadStack->Mark(0);
      lockEndMain = new Semaphore("lock at the end",0);

      for(j = 0; j<lengthBitMap; j++){
        waitOtherThread[j] = new Semaphore("wait executing other thread",0);
      }
      #endif //CHANGED

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
//      Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace ()
{
  // LB: Missing [] for delete
  // delete pageTable;
  delete [] pageTable;

  #ifdef CHANGED
  unsigned int i;
  for (i = 0; i < numPages; i++){
    machine->myFrameProvider->ReleaseFrame(pageTable[i].physicalPage);
  }
   delete bitmapThreadStack;
   delete lockEndMain;
   delete [] waitOtherThread;
  #endif //CHANGED
  // End of modification
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
//      Set the initial values for the user-level register set.
//
//      We write these directly into the "machine" registers, so
//      that we can immediately jump to user code.  Note that these
//      will be saved/restored into the currentThread->userRegisters
//      when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters ()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister (i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister (PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister (NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister (StackReg, numPages * PageSize - 16);
    DEBUG ('a', "Initializing stack register to %d\n",
	   numPages * PageSize - 16);
}


#ifdef CHANGED
int AddrSpace::BeginPointStack(){
    int find = bitmapThreadStack->Find();

    ASSERT(find != -1 );
    currentThread->SetIdThread(find);
    return numPages*PageSize - find*PagePerThread*PageSize;
}

void AddrSpace::DealloateMapStack(){
  bitmapThreadStack->Clear(currentThread->GetIdThread());
}

void AddrSpace::LockEndMain(){
  lockEndMain->P();
}

void AddrSpace::FreeEndMain(){
  lockEndMain->V();
}

void AddrSpace::LockIdThread(int id){
  waitOtherThread[id]->P();
}

void AddrSpace::FreeIdThread(int id){
  waitOtherThread[id]->V();
}

int AddrSpace::NbreThread(){
  return bitmapThreadStack->NbBitAt1();
}
#endif //CHANGED

//----------------------------------------------------------------------
// AddrSpace::SaveState
//      On a context switch, save any machine state, specific
//      to this address space, that needs saving.
//
//      For now, nothing!
//----------------------------------------------------------------------

void
AddrSpace::SaveState ()
{
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
//      On a context switch, restore the machine state so that
//      this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void
AddrSpace::RestoreState ()
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

#ifdef CHANGED
static void ReadAtVirtual( OpenFile *executable, int virtualaddr,
int numBytes, int position, TranslationEntry *pT, unsigned numPages) {

  // On appelle ReadAt et on stocke le résultat (données lues) dans un buffer local
  char buffer[numBytes];
  int size = executable->ReadAt(buffer, numBytes, position);

  ASSERT(numBytes==size);
  ASSERT((unsigned)virtualaddr+size < numPages*PageSize);
  // On recopie le buffer dans la mémoire virtuelle

  // On sauvegarde la table des pages avant d'appeller WriteMem
  TranslationEntry *oldPageTable = machine->pageTable;
  unsigned oldNumPages = machine->pageTableSize;
  machine->pageTable = pT; 
  machine->pageTableSize = numPages;

  // On écrit dans la mémoire virtuelle via WriteMem
  int i;
  for(i=0;i<size;i++) {
    machine->WriteMem(virtualaddr+i, 1, *(buffer+i));
  }

  // On restore l'ancienne table des pages
  machine->pageTable = oldPageTable;
  machine->pageTableSize = oldNumPages;
}

TranslationEntry*
AddrSpace::getPageTable()
{
  return pageTable;
}

void
AddrSpace::setPageTable(TranslationEntry *t)
{
  pageTable = t;
}

#endif
