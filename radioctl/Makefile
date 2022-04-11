$(OUTDIR)/$(TARGET): main.m $(OUTDIR)
	xcrun -sdk iphoneos clang -arch arm64 -o radioctl main.m -framework Foundation -framework MobileCoreServices
	./ldid -Ksigncert.p12 -Sentitlements.plist ./radioctl
