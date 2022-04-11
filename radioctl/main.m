#include <stdio.h>

#import <objc/runtime.h>
#include <UIKit/UIKit.h>
#include <dlfcn.h>

int main(int argc, char *argv[], char *envp[]) {
	@autoreleasepool {
        if(argc<2) return -1;
        
        dlopen("/System/Library/PrivateFrameworks/AppSupport.framework/AppSupport", RTLD_NOW);
        
        Class rpclz = objc_getClass(("RadiosPreferences"));
        NSLog(@"RadiosPreferences=%x", rpclz);
        id rp = [[rpclz alloc] init];
        NSLog(@"RadiosPreferences=%x", rp);
        
        id state = atol(argv[1])==0 ? nil : [NSNumber numberWithBool:YES];
        [rp performSelector:@selector(setAirplaneMode:) withObject:state];
        
        [rp performSelector:@selector(synchronize)];
        
	}
    
    return 0;
}
