// Allocate an integer on the heap
int* pVal = new int(42); 

// Allocate an array of integers on the heap
int* pArr = new int[100]; 

// Cleanup (Must match! array allocation requires delete[])
delete pVal;
delete[] pArr;
