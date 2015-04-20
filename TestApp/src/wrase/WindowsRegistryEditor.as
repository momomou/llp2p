package wrase   {
	
	import wrase.model.WindowsRegistryDefaults;
	
	/**
	 * Manages the loading, parsing/conversion, and updating of the Windows Registry via the Microsoft Windows Registry Editor (regedit.exe). 
	 * Registry key data is stored within instances of WindowsRegistryKey, and each WindowsRegistryKey instance manages instances 
	 * of WindowsRegistryEntry which is the actual data.
	 * 
	 * Note that child WindowsRegistryKey instances may not necessarily be related to each other (may not be children or sibblings, 
	 * for example).
	 * 
	 * In the example below, "HKEY_CURRENT_CONFIG\test" is the key (WindowsRegistryKey), while "teststring", "testbinary", "testdword",  
	 * "testword", "testmultistring", and "testexpandablestring" are all child entries (WindowsRegistryEntry):
	 * 
	 * [HKEY_CURRENT_CONFIG\test]
	 * "teststring"="Some test data with a quote!"
	 * "testbinary"=hex:11,10,10,10
	 * "testdword"=dword:0000ffff
	 * "testword"=hex(b):e0,12,0e,01,00,00,00,00
	 * "testmultistring"=hex(7):65,00,77,00,3b,00,72,00,6b,00,6c,00,6a,00,77,00,65,00,\
	 * 3b,00,72,00,6c,00,6b,00,00,00,77,00,65,00,72,00,3b,00,77,00,6b,00,65,00,72,\
	 * 00,00,00,77,00,6b,00,65,00,72,00,3b,00,77,00,65,00,6b,00,72,00,3b,00,77,00,\
	 * 6b,00,65,00,72,00,00,00,77,00,65,00,3b,00,72,00,6c,00,6b,00,77,00,00,00,3b,\
	 * 00,72,00,65,00,6b,00,77,00,3b,00,65,00,6c,00,6b,00,72,00,00,00,77,00,6b,00,\
	 * 3b,00,6c,00,72,00,65,00,00,00,77,00,3b,00,6b,00,65,00,6c,00,72,00,77,00,65,\
	 * 00,72,00,00,00,00,00
	 * "testexpandablestring"=hex(2):65,00,77,00,72,00,77,00,65,00,72,00,77,00,65,00,\
	 * 72,00,77,00,65,00,72,00,77,00,65,00,72,00,00,00
	 * 
	 * ! IMPORTANT !
	 * 
	 * Your application descriptor XML (application.xml), must include Extended Desktop (native) profile support:
	 * 
	 * <supportedProfiles>extendedDesktop</supportedProfiles>
	 * 
	 * WindowsRegistryEditor will fail when attempting to invoke some native processes (throwing error 3219), if this is not done.
	 * 
	 * _ NOTE _
	 * 
	 * Because Microsoft Windows Registry Editor is a privileged process, the user may be presented with a security dialog when the 
	 * registry is loaded or updated unless the application is already running with Administrator privileges.
	 * 
	 * @author Patrick Bay
	 * 
	 * @see https://support.microsoft.com/kb/310516
	 * 
	 * The MIT License (MIT)
	 * 
	 * Copyright (c) 2014 Patrick Bay
	 * 
	 * Permission is hereby granted, free of charge, to any person obtaining a copy
	 * of this software and associated documentation files (the "Software"), to deal
	 * in the Software without restriction, including without limitation the rights
	 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	 * copies of the Software, and to permit persons to whom the Software is
	 * furnished to do so, subject to the following conditions:
	 * 
	 * The above copyright notice and this permission notice shall be included in
	 * all copies or substantial portions of the Software.
	 * 
	 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	 * THE SOFTWARE.
	 */
	
	import flash.filesystem.File;
	import flash.filesystem.FileStream;
	import flash.filesystem.FileMode;
	import flash.events.FileListEvent;
	import flash.events.NativeProcessExitEvent;
	import flash.events.EventDispatcher;
	import flash.events.ProgressEvent;
	import flash.desktop.NativeApplication;
	import flash.desktop.NativeProcess;
	import flash.desktop.NativeProcessStartupInfo;
	import wrase.model.WindowsRegistryEntry;
	import wrase.model.WindowsRegistryKey;	
	import wrase.events.WindowsRegistryEvent;
	
	
	public class WindowsRegistryEditor extends EventDispatcher {
		
		private var _winCmdExecutable:String = "app-storage:/regedit.cmd"; //the .cmd (newer .bat) file to be used to run privileged processes (like regedit.exe)
		private var _regEditStdExecutable:String = "reg.exe"; //the standard registry editing executable. Typically doesn't need a path as it's a Windows component.
		private var _regEditAdmExecutable:String = "regedit.exe"; //the administrative registry editing executable. Executing this will cause the UAC dialog to be displayed.
		
		private var _winCmdFile:File = null; //the File reference used in conjunction with the _winCmdExecutable
		private var _winCmdProcess:NativeProcess; //the NativeProcess used to launch and control the .cmd process
		private var _winCmdProcessInfo:NativeProcessStartupInfo; //the NativeProcessStartupInfo used to start the .cmd process
		private var _registryKeys:Vector.<WindowsRegistryKey> = new Vector.<WindowsRegistryKey>(); //all the child keys managed by this instance
		private var _registryOutputPath:String = "app-storage:/registry_output.reg"; //the .reg file that regedit.exe generates
		private var _registryInputPath:String = "app-storage:/registry_input.reg";	//the .reg file supplied to regedit.exe for updates.	
		
		public static const regOutputFileEncoding:String = "utf-16"; //output .reg file encoding
		public static const regInputFileEncoding:String = "utf-8"; //input .reg file encoding
		public static const cmdFileEncoding:String = "iso-8895-1"; //output .cmd file encoding
		public static const regFileHeader:String = "Windows Registry Editor Version 5.00"; //MS Windows Registry Editor header (first line of all .reg files)
		
		private static const CR:String = String.fromCharCode(13); //carriage return
		private static const LF:String = String.fromCharCode(10); //line feed
		private static const CRLF:String = CR + LF; //carriage return + line feed
		private static const doubleCRLF:String = CRLF + CRLF; //double carriage return + line feed
		
		private static const WHITESPACE_RANGE:String = " -_\\/\n\r"+String.fromCharCode(13) + String.fromCharCode(10) + String.fromCharCode(9);	//white space / control characters
		
		private var _useAdminPrivileges:Boolean = false; //loads
		private var _parseTopOnly:Boolean = false;	//if true, only the top (first) key found in the loaded/supplied data is parsed, otherwise all keys are processed
		
		/**
		 * Creates a new WindowsRegistryEditor instance.
		 */
		public function WindowsRegistryEditor() {			
		}//constructor
		
		/**
		 * Attempts to load a Windows registry from a specified root key, optionally including all child keys and data. 
		 * Since this is a privileged operation, it is launched via a batch file and is subject to the user accepting 
		 * the security dialog (unless the application is already running with privileges).		 
		 * Be aware that this operation also loads all child keys, which means that it's advisable to prevent
		 * loadng high-level registry keys such as "HKEY_CURRENT_USER".
		 * 
		 * @param	rootKey The root Windows registry key to load (all children are automatically loaded as well). 
		 * For example, "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings" (don't
		 * forget to escape any special characters like slashes ("HKEY_CURRENT_USER\\Software\\...");
		 * @param	useAdminPrivileges Loads registry data using administrative privileges (user will be presented with UAC dialog).
		 * Most root registry keys except "HKEY_CURRENT_USER" require this to be true (but this may differ on your system).
		 * @param   excludeChildren Parses only the top (specified key) if false, otherwise key and all children will be parsed.
		 * 
		 */
		public function loadRegistry(rootKey:String, useAdminPrivileges:Boolean=false, excludeChildren:Boolean = true):void {	
			this._parseTopOnly = !excludeChildren;
			this._useAdminPrivileges = useAdminPrivileges;
			try {
				this._winCmdProcess = new NativeProcess();
				var outputFile:File = new File(this._registryOutputPath);
				var startInfo:NativeProcessStartupInfo = this.generateRegistryReadByBatchInfo(rootKey, outputFile, this._winCmdExecutable, this._useAdminPrivileges);
				this._winCmdProcess.addEventListener(NativeProcessExitEvent.EXIT, this.onLoadRegistry);
				this._winCmdProcess.start(startInfo);
			} catch (err:*) {
				var errEvent:WindowsRegistryEvent = new WindowsRegistryEvent(WindowsRegistryEvent.ONLOADERROR);
				this.dispatchEvent(errEvent);
			}//catch
		}//loadRegistry
		
		/**
		 * Updates the Windows registry using the data associated with this WindowsRegistryEditor instance
		 * (its child WindowsRegistryKey and WindowsRegistryEntry instances).		 
		 * It is advisable to make all desired registry changes before incoking this method as all changes
		 * are applied at once.
		 * Since this is a privileged operation, it is launched via a batch file and is subject to the user accepting 
		 * the security dialog (unless the application is already running with privileges).	
		 * 
		 * @param	useAdminPrivileges Updates registry data using administrative privileges (user will be presented with UAC dialog).
		 * Most root registry keys except "HKEY_CURRENT_USER" require this to be true (but this may differ on your system).
		 */
		public function updateRegistry(useAdminPrivileges:Boolean = false):void {	
			this._useAdminPrivileges = useAdminPrivileges;
			var registryUpdateData:String = regFileHeader + doubleCRLF;
			registryUpdateData += this.toString();
			var inputFile:File = new File(this._registryInputPath);
			var startInfo:NativeProcessStartupInfo = this.generateRegistryUpdateByBatchInfo(registryUpdateData, inputFile, this._winCmdExecutable, this._useAdminPrivileges);			
			this._winCmdProcess = new NativeProcess();
			this._winCmdProcess.addEventListener(NativeProcessExitEvent.EXIT, this.onRegistryUpdateComplete);
			this._winCmdProcess.start(startInfo);				
		}//updateRegistry
		
		/**
		 * Produces a string representation of all of the child keys associated with this instance in Windows Registry 
		 * Editor 5.0 format.
		 * 
		 * @returna A string representation of all of the child keys associated with this instance in Windows Registry 
		 * Editor 5.0 format. Note that the data listed may not necessarily be child (or even related) values. This allows
		 * for flexibility in updating the registry but also 
		 */
		override public function toString():String {
			var returnString:String = new String();
			for (var count:uint = 0; count < this._registryKeys.length; count++) {
				var currentKey:WindowsRegistryKey = this._registryKeys[count];
				if (currentKey!=null) {
					returnString += currentKey.toString() + CRLF;
				}//if
			}//for
			returnString += doubleCRLF;
			return (returnString);
		}//toString
		
		/**
		 * Parses a raw registry string containing one or more registry keys and their associated child entries. Direct output 
		 * from a registry extract (load) operation -- a standard .reg file -- is an example of the type of data to be used with this method.
		 * 
		 * @param	rawRegistryData The raw registry data, in Microsoft Windows Registry Editor 5.0 format, to parse.
		 * 
		 * @return A WindowsRegistryKey instance representing the first key found in the parsed registry data (other keys
		 * may also be present), or null if there was an error parsing the data.
		 */
		public function parseRawRegistry (rawRegistryData:String = null):WindowsRegistryKey {
			if (rawRegistryData == null) {
				return (null);
			}//if
			return (this.parseRegistryInfo(rawRegistryData));
		}//parseRawRegistry
		
		/**
		 * Attempts to retrieve a child WindowsRegistryKey instance based on a specific key path.
		 * 
		 * @param	keyPath The key path to search for (for example, "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion"). Don't
		 * forget to escape all back-slashes in this value first ("HKEY_CURRENT_USER\\Software\\Microsoft\\...");
		 * 
		 * @return The first found child WindowsRegistryKey matching the supplied key path, or null if none was found.
		 */
		public function getKeyByPath(keyPath:String):WindowsRegistryKey {			
			for (var count:uint = 0; count < this._registryKeys.length; count++) {
				var currentKey:WindowsRegistryKey = this._registryKeys[count];								
				if (currentKey.keyPath == keyPath) {
					return (currentKey);
				}//if
			}//getKey
			return (null);
		}//getKeyByPath
		
		/**
		 * Returns all keys that are children of a specific parent WindowsRegistryKey instance.
		 * 
		 * @param	rootKey The WindowsRegistryKey that represents the root key. All keys that contain
		 * this path exactly but are longer are considered child keys.
		 * 
		 * @return A vector array of all children WindowsRegistryKey that are considered to be children
		 * of the specified root key, or null if there was an error retrieving this list.
		 */
		public function getChildKeys(rootKey:WindowsRegistryKey):Vector.<WindowsRegistryKey> {			
			if (rootKey == null) {
				return (null);
			}//if
			if ((rootKey.keyPath == null) || (rootKey.keyPath=="")) {
				return (null);
			}//if
			var returnVec:Vector.<WindowsRegistryKey> = new Vector.<WindowsRegistryKey>();
			for (var count:uint = 0; count < this._registryKeys.length; count++) {
				var currentKey:WindowsRegistryKey = this._registryKeys[count];				
				try {					
					if ((currentKey.keyPath.indexOf(rootKey.keyPath) > -1) && (currentKey.keyPath!=rootKey.keyPath)) {					
						returnVec.push(currentKey);
					}//if
				} catch (err:*) {
				}//catch				
			}//for
			return (returnVec);
		}//get childKeys
		
		/**
		 * Appends a new WindowsRegistryKey instance to this instance.
		 * 
		 * @param	keyRef The new WindowsRegistryKey to append to this instance.
		 * 
		 */
		public function appendKey(keyRef:WindowsRegistryKey):void {			
			if (keyRef == null) {
				return;
			}//if
			this._registryKeys.push(keyRef);
		}//get appendKey
		
		/**
		 * Removes a new WindowsRegistryKey instance from this instance.
		 * 
		 * @param	keyRef The new WindowsRegistryKey to remove from this instance.
		 * 
		 */
		public function removeKey(keyRef:WindowsRegistryKey):void {			
			if (keyRef == null) {
				return;
			}//if
			var returnVec:Vector.<WindowsRegistryKey> = new Vector.<WindowsRegistryKey>();
			for (var count:uint = 0; count < this._registryKeys.length; count++) {
				var currentKey:WindowsRegistryKey = this._registryKeys[count];				
				if (keyRef != currentKey) {
					returnVec.push(currentKey);
				}//if
			}//for
			this._registryKeys = returnVec;
		}//get removeKey
		
		/**
		 * Invoked when the native process (.cmd) managing the Microsoft Windows Registry Editor update functionality completes.
		 * 
		 * @param	eventObj A standard NativeProcessExitEvent event object.
		 */
		private function onRegistryUpdateComplete(eventObj:NativeProcessExitEvent):void {
			this._winCmdProcess.removeEventListener(NativeProcessExitEvent.EXIT, this.onRegistryUpdateComplete);
			this._winCmdProcess = null;
			if (eventObj.exitCode == 1) {
				var newEvent:WindowsRegistryEvent = new WindowsRegistryEvent(WindowsRegistryEvent.ONUPDATEERROR);
				newEvent.loadedKey = new WindowsRegistryKey();
				this.dispatchEvent(newEvent);
				return;
			} else {
				//no problem
			}//else
			try {
				var inputFile:File = File.applicationStorageDirectory.resolvePath(this._registryInputPath);
				var batchFile:File = File.applicationStorageDirectory.resolvePath(this._winCmdExecutable);				
				batchFile.deleteFile();
				inputFile.deleteFile();
			} catch (err:*) {				
			}//catch			
		}//onRegistryUpdateComplete
		
		/**
		 * Invoked when the native process (.cmd) managing the Microsoft Windows Registry Editor load functionality completes.
		 * 
		 * @param	eventObj A standard NativeProcessExitEvent event object.
		 */
		private function onLoadRegistry(eventObj:NativeProcessExitEvent):void {	
			this._winCmdProcess.removeEventListener(NativeProcessExitEvent.EXIT, this.onLoadRegistry);
			this._winCmdProcess = null;
			if (eventObj.exitCode == 1) {
				var newEvent:WindowsRegistryEvent = new WindowsRegistryEvent(WindowsRegistryEvent.ONLOADERROR);
				newEvent.loadedKey = new WindowsRegistryKey();
				this.dispatchEvent(newEvent);
				return;
			} else {
				//no problem
			}//else
			var readProcess:NativeProcess = eventObj.target as NativeProcess;
			readProcess.removeEventListener(NativeProcessExitEvent.EXIT, this.onLoadRegistry);			
			var outputFile:File = new File(this._registryOutputPath);
			if (!outputFile.exists) {
				newEvent = new WindowsRegistryEvent(WindowsRegistryEvent.ONLOADERROR);
				newEvent.loadedKey = new WindowsRegistryKey();
				this.dispatchEvent(newEvent);
				return;
			}//if
			var stream:FileStream = new FileStream();			
			stream.open(outputFile, FileMode.READ);
			stream.position = 2; //Skip BOM header...		
			var regFileOutput:String = stream.readMultiByte(stream.bytesAvailable, regOutputFileEncoding);			
			stream.close();			
			try {
				var batchFile:File = File.applicationStorageDirectory.resolvePath(this._winCmdExecutable);
				batchFile.deleteFile();
				outputFile.deleteFile();
			} catch (err:*) {				
			}//catch					
			newEvent = new WindowsRegistryEvent(WindowsRegistryEvent.ONLOAD);
			newEvent.loadedKey = this.parseRegistryInfo(regFileOutput);			
			this.dispatchEvent(newEvent);
		}//onLoadRegistry
		
		/**
		 * Parses the supplied string, assumed to be in Microsoft Windows Registry Editor 5.0 format, into
		 * native values by creating child WindowsRegistryKey instance (which creates WindowsRegistryEntry
		 * instances).
		 * 
		 * @param rawRegInfo The raw registry information to parse. This should be in Microsoft Windows Registry Editor 5.0 format,
		 * such as data that would be generated during a registry export (load) process into a .reg file.
		 * 
		 * @return The top, or first WindowsRegistryKey instance found and correctly parsed from the supplied data (other keys
		 * may have been parsed too!), or null if there was a parsing error.
		 */
		private function parseRegistryInfo(rawRegInfo:String):WindowsRegistryKey {
			var regSplit:Array = rawRegInfo.split(CR);
			var completeEntry:String = new String();
			var returnKey:WindowsRegistryKey = null;
			for (var count:uint = 0; count < regSplit.length; count++) {
				var currentRegEntry:String = regSplit[count] as String;
				if (this.stripChars(currentRegEntry, WHITESPACE_RANGE).length>1 ) {
					completeEntry += currentRegEntry+CR;
				} else {
					try {
						var parsedKey:WindowsRegistryKey = new WindowsRegistryKey(completeEntry);						
						if ((parsedKey.keyPath != null) && (parsedKey.keyPath != "")) {							
							if (returnKey == null) {
								//We're only returning the first key parsed (the root key)
								returnKey = parsedKey;
							}//if							
							this._registryKeys.push(parsedKey);							
							if (this._parseTopOnly) {								
								this._parseTopOnly = false;
								return (returnKey);
							}//if							
						}//if
					} catch (err:*) {					
					} finally {
						completeEntry = new String();
					}//finally
				}//else
			}//for			
			return (returnKey);
		}//parseRegistryInfo
		
		/**
		 * Generates a NativeProcessStartupInfo to be used with a NativeProcess to launch a registry read (extract / load) operation via a batch 
		 * (.cmd) file. The batch file is generated and included with the startup info so in most cases the process just needs to be
		 * started. The resulting .cmd file should not be assumed to exist indefinitely and should be invoked as soon as this method completes.
		 * 
		 * @param	key	The key path of the top registry key to export / load / read from the registry.
		 * @param	registryOutput	A File instance refering to the registry output file (.reg) to be generated by the export / load / read process.
		 * @param	batPath A File instance refering to the batch file (.cmd) that will be created in order to launch the  export / load / read process.
		 * @param	adminPrivileges If true, the administrative windows registry executable is used (will cause a UAC dialog to be shown to user). This
		 * usually needs to be true for all Windows Registry keys other than "HKEY_CURRENT_USER" (but this may not always be the case).
		 * 
		 * @return The NativeProcessStartupInfo to be used with a NativeProcess instance that will invoke an export / load / read operation
		 * with Microsoft Windows Registry editor via a Windows batch (.cmd) file.
		 */
		private function generateRegistryReadByBatchInfo(key:String, registryOutput:File, batPath:String = null, adminPrivileges:Boolean=false):NativeProcessStartupInfo {
			try {
				if ((batPath == null) || (batPath == "")) {
					batPath = this._winCmdExecutable;
				}//if
				var batchContents:String = new String();
				var resolvedOutput:File = registryOutput;
				var resolvedBatchPath:File = File.applicationDirectory.resolvePath(batPath);
				if (resolvedBatchPath.exists) {
					resolvedBatchPath.deleteFile();
				}//if
				//Add "/y" switch (overwrite) just in case output file already exists...
				if (adminPrivileges) {
					batchContents = this._regEditAdmExecutable + " /E \"" + resolvedOutput.nativePath + "\" \"" + key + "\"";
				} else {
					batchContents = this._regEditStdExecutable + " export \"" + key + "\" \"" + resolvedOutput.nativePath + "\" /y"; // "/y"=overwrite if exists
				}//else
				var stream:FileStream = new FileStream();
				stream.open(resolvedBatchPath, FileMode.WRITE);
				stream.writeMultiByte(batchContents, cmdFileEncoding);
				stream.close();
				var processInfo:NativeProcessStartupInfo = new NativeProcessStartupInfo();
				processInfo.executable = resolvedBatchPath;
				return (processInfo);
			} catch (err:*) {				
				return (null);
			}//catch
			return (null);
		}//generateRegistryReadByBatchInfo
		
		/**
		 * Generates a NativeProcessStartupInfo to be used with a NativeProcess to launch a registry update (create / change / delete) operation via a batch 
		 * (.cmd) file. The batch file is generated and included with the startup info so in most cases the process just needs to be
		 * started. The resulting .cmd file should not be assumed to exist indefinitely and should be invoked as soon as this method completes.
		 * 
		 * @param	registryData	The registry data, in Microsoft Windows Registry Editor 5.0 format, to update in the Windows Registry.
		 * @param	regFile	A File instance refering to the registry output file (.reg) into which the registryData data will be written. This file
		 * will then be supplied to the Microsoft Windows Registry Editor 
		 * @param	batPath	A File instance refering to the batch file (.cmd) that will be created in order to launch the  export / load / read process.
		 * 
		 * @return The NativeProcessStartupInfo to be used with a NativeProcess instance to invoke an export / load / read operation
		 * with Microsoft Windows Registry editor via a Windows batch (.cmd) file.
		 */		
		private function generateRegistryUpdateByBatchInfo(registryData:String, regFile:File, batPath:String = null, useAdminPrivileges:Boolean=false):NativeProcessStartupInfo {
			try {
				if ((batPath == null) || (batPath == "")) {
					batPath = this._winCmdExecutable;
				}//if
				var batchContents:String = new String();				
				var resolvedBatchPath:File = File.applicationDirectory.resolvePath(batPath);
				if (resolvedBatchPath.exists) {
					resolvedBatchPath.deleteFile();
				}//if												
				//Create registry update file...
				var stream:FileStream = new FileStream();
				stream.open(regFile, FileMode.WRITE);
				stream.writeMultiByte(registryData, regInputFileEncoding);
				stream.close();
				//Create batch file...
				if (useAdminPrivileges) {
					batchContents = this._regEditAdmExecutable+" /S \"" + regFile.nativePath+"\""; //"/S" = silent
				} else {
					batchContents = this._regEditStdExecutable + " import \"" + regFile.nativePath + "\"";
				}//else				
				stream = new FileStream();
				stream.open(resolvedBatchPath, FileMode.WRITE);
				stream.writeMultiByte(batchContents, cmdFileEncoding);
				stream.close();
				var processInfo:NativeProcessStartupInfo = new NativeProcessStartupInfo();
				processInfo.executable = resolvedBatchPath;
				return (processInfo);
			} catch (err:*) {				
				return (null);
			}//catch
			return (null);
		}//generateRegistryUpdateByBatchInfo		
		
		/**
		 * Strips characters from an input string and returns the resulting string.
		 * 
		 * @param	inputString The input string from which to strip characters. A copy will be made of
		 * this string so that the original will remain unchanged.
		 * @param	stripCharsRange The character(s) to strip from inputString. This parameter may be
		 * an Array of strings or a String.
		 * 
		 * @return A copy of the input string with the specified characters stripped out.
		 */
		private function stripChars(inputString:String, stripCharsRange:*=" "):String {
			if (inputString==null) {
				return(new String());
			}//if
			if ((inputString=="") || (inputString.length==0)) {
				return(new String());
			}//if
			if (stripCharsRange==null) {
				return (inputString);
			}//if
			var localstripCharsRange:String=new String();
			if (stripCharsRange is Array) {
				for (var count:uint=0; count<stripCharsRange.length; count++) {
					localstripCharsRange.concat(String(stripCharsRange[count] as String));
				}//for	
			} else if (stripCharsRange is String) {
				localstripCharsRange=new String(stripCharsRange);
			} else {
				return (inputString);
			}//else
			if ((localstripCharsRange=="") || (localstripCharsRange.length==0)) {
				return (inputString);
			}//if
			var localInputString:String=new String(inputString);
			var returnString:String=new String();			
			for (var charCount:Number=(localInputString.length-1); charCount>=0; charCount--) {
				var currentChar:String=localInputString.charAt(charCount);
				if (localstripCharsRange.indexOf(currentChar)<0) {
					returnString=currentChar+returnString;						
				}//if
			}//for
			return (returnString);
		}//stripChars
		
	}//WindowsRegistryEditor class
	
}//package