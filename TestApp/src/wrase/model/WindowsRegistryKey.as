package wrase.model {
	
	/**
	 * Parses, stores, retrieves, and manipulates a windows registry key which may comprise of 0 or more registry entries, stored as instances
	 * of WindowsRegistryEntry.
	 * 
	 * Based on the Windows Registry Editor 5.0 file format specification:
	 * 
	 * http://support.microsoft.com/kb/310516
	 * https://support.microsoft.com/kb/256986
	 * http://en.wikipedia.org/wiki/Windows_Registry#.REG_files
	 * http://www.tomsitpro.com/articles/reg_files-registry_editor-windows_8,2-276-2.html
	 * 
	 * Here is a sample key in its raw form (as would be passed to the constructor or "create" method):
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
	 * @author Patrick Bay
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
	
	import wrase.events.WindowsRegistryEvent;
	
	public class WindowsRegistryKey {
		
		private var _entries:Vector.<WindowsRegistryEntry> = new Vector.<WindowsRegistryEntry>(); //all child entries	
		private var _keyPath:String = null; //the registry key path
		private var _multilineBuffer:String = new String(); //buffer used during parsing
		private var _buffering:Boolean = false; //true if buffering is currently enabled (only used during parsing), false otherwise
		private var _delete:Boolean = false; //true if flagged for deletion, false otherwise
		
		private static const CR:String = String.fromCharCode(13); //carriage return
		private static const LF:String = String.fromCharCode(10); //line feed
		private static const CRLF:String = CR + LF; //carriage return + line feed
		private static const doubleCRLF:String = CRLF + CRLF; // double carriage return + line feed
		
		private static const _maxKeyLength:uint = 255; //https://support.microsoft.com/kb/256986
		
		/**
		 * Constructor for the class. Optionally parses a multiline raw key value such as would be
		 * extracted by the MS Windows Registry Editor into a .reg file. 
		 * 
		 * @param	rawKeyData The raw string data representing the key data to parse at instantiation.
		 * See header for example. If omitted, the create method can be called subsequently.
		 */
		public function WindowsRegistryKey(rawKeyData:String=null) {
			if ((rawKeyData != null) && (rawKeyData != "")) {
				this.parseKeyData(rawKeyData);				
			}//if
		}//constructor
		
		/**
		 * Parses a multiline raw key value such as would be extracted by the MS Windows Registry Editor into a .reg file. 
		 * This method can be called if no raw data is supplied to the constructor.
		 * 
		 * @param	rawKeyData The raw string data representing the key data to parse. See header for example.		 
		 */
		public function create(rawKeyData:String=null):void {
			if ((rawKeyData != null) && (rawKeyData != "")) {
				this.parseKeyData(rawKeyData);				
			}//if
		}//create
		
		/**
		 * The key path of the key instance. For example: "HKEY_CURRENT_USER\Control Panel\Desktop\Colors"
		 */
		public function set keyPath(pathSet:String):void {
			this._keyPath = pathSet;
		}//set keyPath
		
		public function get keyPath():String {
			return (this._keyPath);
		}//get keyPath
		
		/**
		 * The key path of the parent key instance. 
		 * For example, if the current path is "HKEY_CURRENT_USER\Control Panel\Desktop\Colors", the parent path would 
		 * be "HKEY_CURRENT_USER\Control Panel\Desktop".
		 */
		public function get parentKey():String {
			var keySplit:Array = this.keyPath.split("\\");
			keySplit.pop();
			var keyReturn:String = keySplit.join("\\");
			return (keyReturn);
		}//get parentKey
		
		/**
		 * Attempts to find a specific child registry entry by name (the entry's "name" property).
		 * 
		 * @param	entryName The child entry name to match.
		 * 
		 * @return The first WindowsRegistryEntry that matches the specified name, or null if none can be found.
		 */
		public function getEntryByName(entryName:String):WindowsRegistryEntry {
			for (var count:uint = 0; count < this._entries.length; count++) {
				var currentEntry:WindowsRegistryEntry = this._entries[count];
				if (currentEntry.name == entryName) {
					return (currentEntry);
				}//if
			}//for
			return (null);
		}//getEntryByName
		
		/**
		 * Attempts to retrieve a specific child registry entry by its index (position within the key). The total
		 * number of child entries available is available through the numEntries property. All entries are 0-indexed.
		 * 
		 * @param	entryIndex The index of the entry to retrieve.
		 * 
		 * @return The WindowsRegistryEntry at the specified index, or null if none exists.
		 */
		public function getEntryByIndex(entryIndex:uint):* {
			try {
				var currentEntry:WindowsRegistryEntry = this._entries[entryIndex];
				return (currentEntry);				
			} catch (err:*) {
				return (null);
			}//catch
			return (null);
		}//getEntryByIndex
		
		/**
		 * Returns a vector array of all WindowsRegistryEntry instances associated with this key, or
		 * null if none exist.
		 */
		public function get entries():Vector.<WindowsRegistryEntry> {
			return (this._entries);
		}//get entries
		
		/**
		 * Returns the total number of child entries associated with this key. If no entries
		 * exist (entries is null), 0 is returned.
		 */
		public function get numEntries():int {
			if (this._entries == null) {
				return (0);
			}//if
			return (this._entries.length);
		}//get entries
		
		/**		 
		 * Appends a WindowsRegistryEntry instance to this WindowsRegistryKey instance.
		 * 
		 * @param	entryRef The WindowsRegistryEntry instance to append to this WindowsRegistryKey instance.		 
		 */
		public function appendEntry(entryRef:WindowsRegistryEntry):void {
			if (entryRef!=null) {
				this._entries.push(entryRef);
			}//if
		}//appendEntry
		
		/**		 
		 * Removes the specified entry from this key instance. Removal is different from deletion
		 * in that the entry data is simply removed from the key will NOT result in the entry
		 * being deleted from the Windows Registry on an update. So any entries removed
		 * from their parent key will simply be omitted from any Windows Registry updates, which
		 * also prevents them from being deleted. To mark an entry for deletion from the Windows
		 * Registry itself, use the deleteEntry method instead.
		 * 
		 * @param	entryRef The WindowsRegistryEntry instance to remove from this key instance.
		 * 
		 * @return True if the entry was successfully removed, false otherwise (for example, this
		 * key instance is not the entry's parent).
		 */
		public function removeEntry(entryRef:WindowsRegistryEntry):Boolean {
			var compactEntries:Vector.<WindowsRegistryEntry> = new Vector.<WindowsRegistryEntry>();
			var removed:Boolean = false;
			for (var count:uint = 0; count < this._entries.length; count++) {
				var currentEntry:WindowsRegistryEntry = this._entries[count];
				if (currentEntry != entryRef) {
					compactEntries.push(currentEntry);
				} else {
					removed = true;
				}//else
			}//for
			this._entries = compactEntries;
			return (removed);
		}//removeEntry		
		
		/**
		 * Marks the key for deletion. The key remains in memory, and child entries/keys remain
		 * unaffected. The deletion is not applied until an update is made via the associated
		 * WindowsRegistryEditor instance. Child entries/keys will be deleted from the
		 * Windows Registry along with the current key (once applied), so use with caution.		 
		 */
		public function deleteKey():void {
			this._delete = true;
		}//deleteKey
		
		/**
		 * Removes the deletion flag from the key, effectively restoring the key's data, child keys, 
		 * and entries.
		 */
		public function undeleteKey():void {
			this._delete = false;
		}//undeleteKey
		
		/**
		 * Converts the key instance and its entries into a string representation that can be used
		 * to produce a .reg file (in order to update the Windows Registry).
		 * 
		 * @return A Windows Registry Editor 5.0 formatted string representation of the registry key instance,
		 * including all of its child entries.
		 * 
		 * Note that this method doesn't include any child keys; this function should only produce the current
		 * key instance.
		 */
		public function toString():String {
			var returnString:String = new String();
			if (this._delete) {
				returnString = "[-" + this.keyPath + "]" +  String.fromCharCode(13) + String.fromCharCode(10);				
			} else {
				returnString = "[" + this.keyPath + "]" +  String.fromCharCode(13) + String.fromCharCode(10);			
				for (var count:uint = 0; count < this._entries.length; count++) {
					var currentEntry:WindowsRegistryEntry = this._entries[count];				
					returnString += currentEntry.toString() + String.fromCharCode(13) + String.fromCharCode(10);
				}//for			
			}//else
			return (returnString);			
		}//toString
		
		/**
		 * Attempts to set the key path using the specified string. The string is analyzed to
		 * see if matches the Windows Registry Editor 5.0 path (key) format, and sets the
		 * keyPath property if it appears to be valid.
		 * 
		 * @param	entryLine The string to analyze.
		 * 
		 * @return True if the key path was valid and was set to the keyPath value, false otherwise.
		 */
		private function setKeyPath(entryLine:String):Boolean {						
			if ((entryLine.indexOf("[") != 0) || (entryLine.indexOf("]") < 0) || (entryLine.indexOf("\\") < 0)) {								
				return (false);
			}//if
			var entry:String = new String(entryLine);
			entry = entry.split("[").join("");			
			entry = entry.split("]").join("");
			this._keyPath = entry;			
			return (true);
		}//setKeyPath		
		
		/**
		 * Parses the data of an individual key, including its child entries, to native values and
		 * instances.
		 * 
		 * @param	keyData The raw key data string to be analyzed and parsed. See the class header for
		 * an example of a valid Windows Egistry Editor 5.0 raw key string.
		 */
		private function parseKeyData(keyData:String):void {			
			var entryLines:Array = keyData.split(CR);
			this._multilineBuffer = new String();
			this._buffering = false;
			var currentMultilineItem:WindowsRegistryEntry = null;			
			for (var count:uint = 0; count < entryLines.length; count++) {				
				var currentLine:String = new String(entryLines[count] as String);								
				currentLine = currentLine.split(LF).join("");				
				if (this.stripWhiteSpace(currentLine)!="") {
					if ((this.keyPath == null) && (this.setKeyPath(currentLine) == false)) {
						var error:Error = new Error("WindowsRegistryKey error - Registry key not valid: " + currentLine, 1);						
						throw (error);
					} else {
						try {
							if (currentLine.lastIndexOf("\\") == (currentLine.length - 1)) {								
								var bufferLine:String = currentLine.substr(0, (currentLine.length - 1));
								bufferLine = bufferLine.split(String.fromCharCode(32)).join("");
								this._multilineBuffer += bufferLine;
								this._buffering = true;
							} else {								
								if (this._buffering) {																		
									bufferLine = new String(currentLine);
									bufferLine = bufferLine.split(String.fromCharCode(32)).join("");
									this._multilineBuffer += bufferLine;
									this._buffering = false; //final buffer line
								} else {									
									this._buffering = false;
									this._multilineBuffer = currentLine;
								}//else
							}//else
						} catch (err:*) {														
							this._multilineBuffer = new String();
							this._buffering = false;
						}//catch						
						if ((this._multilineBuffer != "") && (!this._buffering)) {								
							try {
								var dataItem:WindowsRegistryEntry = new WindowsRegistryEntry(this._multilineBuffer);							
								if (dataItem.valid) {																	
									dataItem.key = this;																
									this._entries.push(dataItem);																	
									this._multilineBuffer = new String();
									this._buffering = false;
								} else {							
									//Invalid registry value						
								}//else
							} catch (err:*) {
								this._multilineBuffer = new String();
								this._buffering = false;
							}//catch
						} else {
							//Still buffering or buffer is empty
						}//else
					}//else
				}//if
			}//for
		}//parseKeyData
		
		/**
		 * Strips the white space (space, line feed, carriage return, tab), from a specified string. The original
		 * string is left unaffacted.
		 * 
		 * @param	inputStr The string from which to strip white space. A copy of this string will be made so that
		 * the original remains untouched.
		 * 
		 * @return A copy of the input string with white space stripped out.
		 */
		private function stripWhiteSpace(inputStr:String):String {
			var returnStr:String = new String (inputStr);
			returnStr = returnStr.split(String.fromCharCode(32)).join(""); //space
			returnStr = returnStr.split(CR).join("");
			returnStr = returnStr.split(LF).join("");
			returnStr = returnStr.split(String.fromCharCode(9)).join(""); //tab
			return (returnStr);
		}//stripWhiteSpace		
		
	}//WindowsRegistryEntry class
	
}//package