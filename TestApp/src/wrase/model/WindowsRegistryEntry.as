package wrase.model {
	
	/**
	 * Parses, stores, retrieves, and manipulates a windows registry entry (a single data item within a key). Based on the Windows Registry Editor 5.0 file format specification:
	 * 
	 * http://support.microsoft.com/kb/310516
	 * https://support.microsoft.com/kb/256986
	 * http://en.wikipedia.org/wiki/Windows_Registry#.REG_files
	 * http://www.tomsitpro.com/articles/reg_files-registry_editor-windows_8,2-276-2.html
	 * 
	 * Registry data types are mapped to native data types in the following way:
	 * 
	 * type_string <-->   String
	 * type_dword  <-->   int, uint, Number, Boolean
	 * type_hex    <-->   ByteArray
	 * type_hex0   <-->   ByteArray
	 * type_hex1   <-->   ByteArray
	 * type_hex2   <-->   ByteArray, String, XML, XMLList
	 * type_hex4   <-->   ByteArray
	 * type_hex5   <-->   ByteArray
	 * type_hex7   <-->   ByteArray
	 * type_hex8   <-->   ByteArray
	 * type_hexA   <-->   ByteArray
	 * type_hexB   <-->   ByteArray	 
	 * 
	 * Some examples of raw registry entries that could be used with the constructor or "create" method:
	 * 
	 * "teststring"="Some test data with a quote!"
	 * "testbinary"=hex:11,10,10,10
	 * "testdword"=dword:0000ffff
	 * "testword"=hex(b):e0,12,0e,01,00,00,00,00
	 * "testmultistring"=hex(7):65,00,77,00,3b,00,72,00,6b,00,6c,00,6a,00,77,00,65,00,00,00
	 * "testexpandablestring"=hex(2):65,00,77,00,72,00,77,00,65,00,72,00,77,00,65,00,72,00,77,00,65,00,72,00,77,00,65,00,72,00,00,00
	 * 
	 * Note that any line breaks/delimiters for long data must be removed before attempting to parse raw data.
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
	
	import flash.utils.ByteArray;
	
	public class WindowsRegistryEntry 	{
		
		//Windows Registry Editor 5.0 data types
		public static const type_dword:String = "dword"; //integer value (4 bytes)
		public static const type_hex:String = "hex"; //comma-separated list of hex values
		public static const type_hex0:String = "hex(0)"; //same as type_hex
		public static const type_hex1:String = "hex(1)"; //comma separated list of UTF-16LE data (null-terminated string) -  http://www.fileformat.info/info/charset/UTF-16LE/list.htm
		public static const type_hex2:String = "hex(2)"; //variable length string represented as comma-separated hex values (data includes variable denoting string length)
		public static const type_hex4:String = "hex(4)"; //comma-separated list of DWORD (4 byte) hex values in little endian order (not fully tested)
		public static const type_hex5:String = "hex(5)"; //comma-separated list of DWORD (4 byte) hex values in big endian order(not fully tested)		
		public static const type_hex7:String = "hex(7)"; //human-readable string data represented as comma-delimited list of hex values
		public static const type_hex8:String = "hex(8)"; //resource list represented as comma-delimited list of hex values (NOT CURRENTLY SUPPORTED)
		public static const type_hexA:String = "hex(a)"; //resource requirements list represented as comma-delimited list of hex values (NOT CURRENTLY SUPPORTED)
		public static const type_hexB:String = "hex(b)"; //qword value (double d-word), 8 bytes represented in hex in little endian order
		public static const type_string:String = "string"; //interpreted as the default (no type) type
		
		//General-use constants
		private static const CR:String = String.fromCharCode(13);
		private static const LF:String = String.fromCharCode(10);
		private static const CRLF:String = CR + LF;
		private static const doubleCRLF:String = CRLF + CRLF;
		
		//Data encoding types used for string data
		public static const enc_utf16le:String = "utf-16le"; //UTF-16LE string encoding (has BOM header)
		public static const enc_utf16:String = "utf-16"; //UTF-16 string encoding (no BOM header)
		public static const enc_utf8:String = "utf-8"; //UTF-8 string encoding (no BOM header)
		public static const enc_iso88591:String = "iso-8859-1"; //ISO-8859-1 string encoding
		
		//Internal variables storing the entry's information
		private var _type:String = null; //The entry data type, as represented by one of the type_* constants above
		private var _name:String = null; //The entry name (like the variable name of the entry)
		private var _value:* = null; //The entry data (like the contents of the variable, may be any of the types
		private var _parentKey:WindowsRegistryKey = null; //The parent WindowsRegistryKey instance to which this entry belongs		
		private var _delete:Boolean = false; //Entry has been marked for deletion on next update
		
		/**
		 * Creates an instance of WindowsRegistryEntry, optionally initializing it with a string representation of the entry (as would be extracted
		 * from a MS Windows Registry Editor export, for example).
		 * 
		 * @param	entryData The raw registry entry to parse, or null to create an empty instance.
		 */
		public function WindowsRegistryEntry(entryData:String=null) {
			if ((entryData != null) && (entryData != "")) {
				this.parseRawEntryData(entryData);
			}//if
		}//constructor
		
		/**
		 * Creates the windows registry entry from the raw string representation (as would be extracted
		 * from a MS Windows Registry Editor export, for example).
		 * 
		 * @param	rawEntryData A string representation of the registry entry such as would be produced via a
		 * MS Windows Registry Editor export. See examples in class header for details.
		 */
		public function create(rawEntryData:String):void {
			if ((rawEntryData != null) && (rawEntryData != "")) {
				this.parseRawEntryData(rawEntryData);
			}//if
		}//create
		
		/**
		 * A reference to the parent WindowsRegistryKey object. Setting this reference
		 * to another WindowsRegistryKey instance will cause this entry to first be
		 * removed from its current parent key and then appended to the new parent key.
		 * Setting this value to null creates an orphan entry.
		 */
		public function set key(keySet:WindowsRegistryKey):void {
			if ((keySet!=null) && (this._parentKey!=null)) {
				if (keySet != this._parentKey) {
					//The same entry could theoretically belong to more than one key. But not today...
					this._parentKey.removeEntry(this);
					keySet.appendEntry(this);
				}//if
			}//if
			this._parentKey = keySet;
		}//set key
		
		public function get key():WindowsRegistryKey {
			return (this._parentKey);
		}//get key
		
		/**
		 * Returns true if the current entry is considered valid (is not marked for deletion, has a valid name, 
		 * valid data, and a valid type), false otherwise.
		 */
		public function get valid():Boolean {
			if ((this._value == null) || (this._name == null) && (this._name == "") || (this.isTypeValid(this._type) == false) || (this._delete)) {
				return (false);
			}//if
			return (true);
		}//get valid
		
		/**
		 * Returns the entry's name (like a variable name), or null if not yet set.
		 */
		public function get name():String {
			return (this._name);
		}//get name
		
		public function set name(nameSet:String):void {
			this._name = nameSet;
		}//set name
		
		/**
		 * The entry's value (like a variable's contents), or null if not yet set. Be aware that this
		 * value may be any one of the native Flash data types depending on how the value has been set and/or
		 * parsed. If setting the value with an unsupported data type (String, Number, Boolean, ByteArray, uint, 
		 * int, XML, XMLList), an error will be thrown thrown.		 		
		 */
		public function set value(valueSet:*):void {
			//TODO: Seems like we should in theory be able to support other Flash objects too (MovieClip, for example).
			if ((this.getDataType(valueSet) == "object") || (this.getDataType(valueSet) == "null")) {
				var error:Error = new Error("WindowsRegistryEntry: Attempt to apply invalid data type (\"" + this.getDataType(valueSet) + "\") on property \"value\".", 2);
				throw(error);
				return;
			}//if
			var previousType:String = this.getDataType(this._value);
			this._value = valueSet;
			var currentType:String = this.getDataType(this._value);
			if ((this._type==null) || (this._type=="") || (previousType!=currentType)) {
				if ((this._value is int) || (this._value is uint) || (this._value is Number) || (this._value is Boolean)) {
					this.type = type_dword;
				} else if (this._value is ByteArray) {
					this.type = type_hex;
				} else {
					//Should cover most other data?
					this.type = type_hex2;
				}//else
			}//if			
		}//get value
		
		public function get value():* {
			return (this._value);
		}//get value
		
		/**
		 * Marks the entry for deletion. When deleted, the entry is represented as: "name":-
		 * However, the name, value, and type remain stored in memory to allow the entry to
		 * be undeleted.
		 * 
		 * An entry is not actually deleted in the Windows Registry until an update is invoked.
		 */
		public function deleteEntry():void {
			this._delete = true;
		}//deleteEntry
		
		/**
		 * Restores the entry if previously marked for deletion. Does nothing
		 * if entry was not marked for deletion.
		 */
		public function undeleteEntry():void {
			this._delete = false;
		}//undeleteEntry		
		
		/**
		 * The data type of the registry entry. This type is forced to one of the
		 * data type constants of the WindowsRegistryEntry class (type_dword, type_hex, etc.)
		 * In other words, attempting to set this value to anything other than one of the data type
		 * constants will result in the type being set to the default type, which is type_hex2.
		 */
		public function set type(typeSet:String):void {
			if (typeSet == null) {
				return;
			}//if
			switch (typeSet.toLowerCase()) {
				case type_string: this._type = type_string; break;
				case type_dword: this._type = type_dword; break;
				case type_hex: this._type = type_hex; break;
				case type_hex0: this._type = type_hex0; break;
				case type_hex1: this._type = type_hex1; break;
				case type_hex2: this._type = type_hex2; break;
				case type_hex4: this._type = type_hex4; break;
				case type_hex5: this._type = type_hex5; break;
				case type_hex7: this._type = type_hex7; break;
				case type_hex8: this._type = type_hex8; break;
				case type_hexA: this._type = type_hexA; break;
				case type_hexB: this._type = type_hexB; break;
				default: this._type = type_hex2; break;
			}//switch
		}//set type
		
		public function get type():String {
			return (this._type);
		}//get type
		
		/**
		 * Validates the specified type string against the valid data types
		 * that can be represented by the entry (for example type_dword, type_hex, etc.)
		 * The type isn't case-sensitive but this method will return false if
		 * the supplied type is invalid in any other way.
		 * 
		 * @param	typeSet The type string to analyze for validity.
		 * 
		 * @return True if the type string is recognized as a valid data type for
		 * the entry, false otherwise.
		 */
		public function isTypeValid(typeSet:String):Boolean {			
			if (typeSet == null) {
				return (false);
			}//if
			switch (typeSet.toLowerCase()) {
				case type_string: return (true); break;
				case type_dword: return (true); break;
				case type_hex: return (true); break;
				case type_hex0: return (true); break;
				case type_hex1: return (true); break;
				case type_hex2: return (true); break;
				case type_hex4: return (true); break;
				case type_hex5:  return (true); break;
				case type_hex7:  return (true); break;
				case type_hex8: return (true); break;
				case type_hexA: return (true); break;
				case type_hexB: return (true); break;
				default: return (false); break;
			}//switch	
			return (false);
		}//isTypeValid
		
		/**
		 * Produces a string representation of the entry. This representation is
		 * similar to what would be produced by the MS WIndows Registry Editor (and can therefore
		 * be used to produce valid .reg file content).
		 * 
		 * @return The Microsoft Windows Registry Editor 5.0 string representation of the registry entry.
		 */
		public function toString():String {			
			if (this.valid == false) {
				return (null);
			}//if
			var returnString:String = new String();
			if (this._delete) {
				returnString = "\"" + this.name + "\"=-";
				return (returnString);
			}//if			
			returnString = "\"" + this.name + "\"" + "=";
			if (this.type == type_string) {
				returnString += "\"" + this.value + "\"";
			} else {
				returnString += this.type + ":";				
				switch (this.type) {
					case type_dword:
						var valueString:String = int(this._value).toString(16);						
						for (var count:int = (7 - valueString.length); count >= 0; count--) {
							returnString += "0";
						}//for
						returnString += valueString;
						break;
					case type_hex:						
						var hexString:String = this.convertNativeValueToHexString(this.value);
						returnString += hexString;
						break;
					case type_hex0:						
						hexString = this.convertNativeValueToHexString(this.value);
						returnString += hexString;
						break;						
					case type_hex2: 
						hexString = this.convertNativeValueToHexString(this.value);
						returnString += hexString;
						break;
					case type_hex4:
						//TODO: Add support for this encoding
						break;
					case type_hex5:
						//TODO: Add support for this encoding
						break;						
					case type_hex7: 
						hexString = this.convertNativeValueToHexString(this.value);
						returnString += hexString;
						break;
					case type_hex8:
						//TODO: Add support for this encoding
						break;								
					case type_hexA:
						//TODO: Add support for this encoding
						break;								
					case type_hexB: 
						hexString = this.convertNativeValueToHexString(this.value);
						returnString += hexString;
						break;	
					default:
						break;
				}//switch
			}//else			
			return (returnString);
		}//toString
		
		/**
		 * Returns the type of the specified data object as a string. Similar to the standard "typeof" function except
		 * that more primitive data types are identified, while items like display objects are not.
		 * 
		 * @param	obj The object to analyze
		 * 
		 * @return The type of data that the object parameter is, or null if not specified. Valid return types 
		 * include: string, number, boolean, uint, int, xml, xmllist, bytearray, object, null
		 */
		private function getDataType(obj:*):String {
			if (obj == null) {
				return ("null");
			}//if
			if (obj is String) {
				return ("string");
			}//if
			if (obj is Number) {
				return ("number");
			}//if
			if (obj is Boolean) {
				return ("boolean");
			}//if
			if (obj is uint) {
				return ("uint");
			}//if
			if (obj is int) {
				return ("int");
			}//if
			if (obj is XML) {
				return ("xml");
			}//if
			if (obj is XMLList) {
				return ("xmllist");
			}//if
			if (obj is ByteArray) {
				return ("bytearray");
			}//if
			return ("object");
		}//getDataType			
		
		/**
		 * Converts a native value / object to a hexadecimal string representation. This type of representation is
		 * used for all registry entry types other than type_string, and varies somewhat depending on the data type
		 * specified for the entry.
		 * 
		 * @param	inputValue The native value or object to convert to a hexademical string representation.
		 * 
		 * @return A comma-delimited (except for type_dword types) hexademical string representation if the input value. The
		 * representation differs based on what the native data type is and what the entry's data type is.
		 */
		private function convertNativeValueToHexString(inputValue:*):String {
			//TODO: Add support for line length (break data into multiple lines like the MS Windows Registry Editor does it).
			var byteValue:ByteArray = new ByteArray();			
			if (inputValue is ByteArray) {					
				byteValue = inputValue;					
			} else if (inputValue is String) {
				if (this.type==type_hex2) {
					byteValue.writeMultiByte(inputValue, enc_utf16le);
					//Add two null characters as per spec
					byteValue.writeByte(0);
					byteValue.writeByte(0);
				} else if (this.type == type_hex7) {					
					byteValue.writeMultiByte(inputValue, enc_utf16le);
					//Just one null here
					byteValue.writeByte(0);						
				} else {
					byteValue.writeMultiByte(inputValue, enc_utf8);
				}//else
			} else if ((inputValue is XML) || (inputValue is XMLList)) {
				if (this.type==type_hex2) {
					byteValue.writeMultiByte(inputValue.toString(), enc_utf16le);					
					byteValue.writeByte(0);
					byteValue.writeByte(0);
				} else if (this.type == type_hex7) {					
					byteValue.writeMultiByte(inputValue.toString(), enc_utf16le);					
					byteValue.writeByte(0);						
				} else {
					byteValue.writeMultiByte(inputValue.toString(), enc_utf8);
				}//else
			} else if (inputValue is int) {								
				byteValue.writeInt(inputValue);
			} else if (inputValue is Number) {					
				byteValue.writeFloat(inputValue);				
			} else if (inputValue is uint) {				
				byteValue.writeUnsignedInt(inputValue);
			} else if (inputValue is Boolean) {
				byteValue.writeBoolean(inputValue);
			} else {
				try {				
					//Try the old toString method when all else fails...
					byteValue.writeMultiByte(inputValue.toString(), enc_utf8);
				} catch (err:*) {					
				}//catch
			}//else
			byteValue.position = 0;
			var returnString:String = new String();
			for (var count:uint = 0; count < byteValue.length; count++) {
				var currentValue:uint = byteValue.readByte() & 0xFF;
				var currentValueString:String = currentValue.toString(16);				
				if (currentValue < 15) {
					currentValueString = "0" + currentValueString;
				}//if				
				returnString += currentValueString + ",";
			}//for
			returnString = returnString.substr(0, (returnString.length - 1)); //strip trailing comma
			if (this.type == type_hexB) {
				//MSB 64-bit value
				returnString = returnString.split(",").reverse().join(",");
			}//if
			return (returnString);
		}//convertNativeValueToHexString
		
		/**
		 * Parses raw entry data to update the entry's name, value, and type, unless the
		 * supplied raw entry data is invalid.
		 * 
		 * @param	entryLine The raw entry data to parse. See the header of this class
		 * for some examples (or assign some data and see the output of an
		 * entry's toString() method).
		 */
		private function parseRawEntryData(entryLine:String):void {			
			//Use this instead of splitting data, in case there are additional quotes
			var varName:String = entryLine.substr(0, entryLine.indexOf("="));
			var varData:String = entryLine.substr(entryLine.indexOf("=") + 1);			
			if (varName.indexOf("\"") < 0) {				
				return;
			}//if
			varName = varName.split("\"").join("");
			this._name = varName;
			if (varData.indexOf("\"") == 0) {
				//This is a string so parse it here...
				this._type = type_string;
				this._value = new String(varData);
				this._value = this._value.substr(1);
				this._value = this._value.substr(0, this._value.length - 1); //strip ending quote
			} else {
				//This is binary data so more processing is needed...
				var dataType:String = varData.substr(0, varData.indexOf(":"));
				var dataStr:String = varData.substr(varData.indexOf(":") + 1);				
				this._type = dataType;
				this._value = this.parseBinaryData(dataType, dataStr);
			}//else			
		}//parseRawEntryData
		
		/**
		 * Parses the binary data of a raw entry string representation. This type of parsing occurs for almost all data types 
		 * except type_string (that is done by the parseRawEntry() method).
		 * 
		 * @param	dataType The type of data (one of the data type constants), that is represented by the data parameter.
		 * @param	data The data to process to a native value.
		 * 
		 * @return A native Flash data value, as translated from the combination of dataType and data parameters. Null
		 * is returned if this operation was unsuccessful.
		 */
		private function parseBinaryData(dataType:String, data:String):* {
			switch (dataType) {
				case type_dword: 
					data = "0x" + data;
					return (new int(data));
					break;
				case type_hexB: 
					var hexDataItems:Array = data.split(",");
					//Items are in LSB/little-endian order, need to be in MSB order for conversion
					hexDataItems = hexDataItems.reverse(); 
					var returnData:ByteArray = new ByteArray();
					for (var count:uint = 0; count < hexDataItems.length; count++) {
						var currentItem:String = hexDataItems[count] as String;
						currentItem = currentItem.split(String.fromCharCode(32)).join("");
						currentItem = "0x" + currentItem;
						returnData.writeByte(uint(currentItem) & 0xFF);						
					}//for
					returnData.position = 0;
					return (returnData);
					break;					
				case type_hex: 
					hexDataItems = data.split(",");
					returnData = new ByteArray();
					for (count = 0; count < hexDataItems.length; count++) {
						currentItem = hexDataItems[count] as String;
						currentItem = currentItem.split(String.fromCharCode(32)).join("");
						currentItem = "0x" + currentItem;
						returnData.writeByte(int(currentItem));						
					}//for
					returnData.position = 0;
					return (returnData);
					break;
				case type_hex0: 
					hexDataItems = data.split(",");
					returnData = new ByteArray();
					for (count = 0; count < hexDataItems.length; count++) {
						currentItem = hexDataItems[count] as String;
						currentItem = currentItem.split(String.fromCharCode(32)).join("");
						currentItem = "0x" + currentItem;
						returnData.writeByte(int(currentItem));						
					}//for
					returnData.position = 0;
					return (returnData);
					break;					
				case type_hex1:					
					hexDataItems = data.split(",");
					var binaryData:ByteArray = new ByteArray();
					for (count = 0; count < hexDataItems.length; count++) {
						currentItem = hexDataItems[count] as String;
						currentItem = currentItem.split(String.fromCharCode(32)).join("");
						currentItem = "0x" + currentItem;
						binaryData.writeByte(int(currentItem));						
					}//for
					binaryData.position = 0;
					return (binaryData.readMultiByte(binaryData.length, enc_utf16le)); //string
					break;		
				case type_hex2: 
					hexDataItems = data.split(",");
					binaryData = new ByteArray();
					count = 0;
					while (count<hexDataItems.length) {
						var byte2:String = hexDataItems[count] as String;
						var byte1:String = hexDataItems[count + 1] as String;	
						byte1 = byte1.split(String.fromCharCode(32)).join("");
						byte2 = byte2.split(String.fromCharCode(32)).join("");
						binaryData.writeByte(int("0x" + byte2));
						binaryData.writeByte(int("0x" + byte1));
						count += 2;
					}//for
					binaryData.position = 0;
					return (binaryData.readMultiByte(binaryData.length, enc_utf16le));
					break;	
				case type_hex4:
					hexDataItems = data.split(",");
					count = 0;
					binaryData = new ByteArray();
					while (count < hexDataItems.length) {
						//LSB order
						byte1 = hexDataItems[count + 3] as String;
						byte2 = hexDataItems[count + 2] as String;						
						var byte3:String = hexDataItems[count + 1] as String;
						var byte4:String = hexDataItems[count] as String;
						byte1 = byte1.split(String.fromCharCode(32)).join("");
						byte2 = byte2.split(String.fromCharCode(32)).join("");
						byte3 = byte3.split(String.fromCharCode(32)).join("");
						byte4 = byte4.split(String.fromCharCode(32)).join("");
						var byteVal:int = new int("0x" + byte1 + byte2 + byte3 + byte4);
						binaryData.writeInt(byteVal);
						count += 4;
					}//while
					return (binaryData);
					break;	
				case type_hex5:
					hexDataItems = data.split(",");
					count = 0;
					binaryData = new ByteArray();
					while (count < hexDataItems.length) {
						//MSB order
						byte1 = hexDataItems[count] as String;
						byte2 = hexDataItems[count + 1] as String;
						byte3 = hexDataItems[count + 2] as String;
						byte4 = hexDataItems[count + 3] as String;
						byte1 = byte1.split(String.fromCharCode(32)).join("");
						byte2 = byte2.split(String.fromCharCode(32)).join("");
						byte3 = byte3.split(String.fromCharCode(32)).join("");
						byte4 = byte4.split(String.fromCharCode(32)).join("");
						byteVal = new int("0x" + byte1 + byte2 + byte3 + byte4);
						binaryData.writeInt(byteVal);
						count += 4;
					}//while
					return (binaryData);
					break;					
				case type_hex7:
					hexDataItems = data.split(",");
					binaryData = new ByteArray();
					count = 0;
					while (count<hexDataItems.length) {
						byte2 = hexDataItems[count] as String;
						byte1 = hexDataItems[count + 1] as String;	
						byte1 = byte1.split(String.fromCharCode(32)).join("");
						byte2 = byte2.split(String.fromCharCode(32)).join("");						
						binaryData.writeByte(int("0x" + byte2));
						binaryData.writeByte(int("0x" + byte1));
						count += 2;
					}//for
					binaryData.position = 0;
					return (binaryData);
					break;							
				default: 
					return (null);
					break;
			}//switch
			return (null);
		}//parseBinaryData
		
	}//WindowsRegistryEntry class
	
}//package