#define K0_TO_PHYS(x) (x)
#define PHYS_TO_K1(x) (x)

#define PHYSADDR(x)  K0_TO_PHYS(x)
#define KERNADDR(x)  PHYS_TO_K0(x)
#define UNCADDR(x)   PHYS_TO_K1(x)
