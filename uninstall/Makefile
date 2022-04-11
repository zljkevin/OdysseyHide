$(OUTDIR)/$(TARGET): main.m $(OUTDIR)
	xcrun -sdk iphoneos clang -arch arm64 -o uninstall main.m -framework Foundation -framework MobileCoreServices
	./ldid -Ksigncert.p12 -Sentitlements.plist ./uninstall
