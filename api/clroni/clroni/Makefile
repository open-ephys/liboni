clroni.dll: oni.cs onidevices.cs
	mcs -unsafe -t:library oni.cs onix.cs Context.cs ContextHandle.cs Device.cs Frame.cs ONIException.cs

.PHONY: clean
clean:
	$(RM) clroni.dll
