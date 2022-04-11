#include <stdio.h>
#import <UIKit/UIKit.h>
#import "LSApplicationWorkspace.h"
#import "LSApplicationProxy.h"
int main(int argc, char *argv[], char *envp[]) {
	@autoreleasepool {
		if (argc < 3) {
			printf("input arg error!\n");
			return 1;
		}
        
        
        if(strcmp(argv[1], "install")==0) {
            NSString *bundleID=[NSString stringWithUTF8String:argv[2]];
            NSString *path=[NSString stringWithUTF8String:argv[3]];
            LSApplicationWorkspace *appWorkspace = [LSApplicationWorkspace defaultWorkspace];
            BOOL flag=[appWorkspace installApplication:[NSURL fileURLWithPath:path]
                                             withOptions:[NSDictionary dictionaryWithObject:bundleID forKey:@"CFBundleIdentifier"]];
            if (flag) {
                NSLog(@"install %@ success!",bundleID);
                return 0;
            }else {
                NSLog(@"install %@ fail!",bundleID);
                return 2;
            }
        }
        
        if(strcmp(argv[1], "uninstall")==0) {
            NSString *bundleID=[NSString stringWithUTF8String:argv[2]];
            LSApplicationWorkspace *appWorkspace = [LSApplicationWorkspace defaultWorkspace];
            BOOL flag=[appWorkspace uninstallApplication:bundleID  withOptions:nil];
            if (flag) {
                NSLog(@"uninstall %@ success!",bundleID);
                return 0;
            }else {
                NSLog(@"uninstall %@ fail!",bundleID);
                return 3;
            }
        }
        
	}
}
