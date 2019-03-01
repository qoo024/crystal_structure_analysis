!include ../rules.mak

libRefinableObj.lib : Tracker.obj Simplex.obj RefinableObj.obj LSQNumObj.obj GlobalOptimObj.obj IO.obj
	tlib "libRefinableObj.lib" -+Tracker.obj -+Simplex.obj -+RefinableObj.obj -+LSQNumObj.obj -+GlobalOptimObj.obj -+IO.obj

lib: libRefinableObj.lib

all: lib
