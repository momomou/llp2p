package wrase.model {
	
	/**
	 * Default values and tests used with the Microsoft Windows Registry.
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
	public class WindowsRegistryDefaults {
		
		//Full key names -- where in doubt, we assume that we need administrator privileges.
		//Create additional, more detailed paths below as required (if you know of keys that definitely require administrator privileges).
		public static const currentUserKeyPath:String = "HKEY_CURRENT_USER"; //Currently logged in user, you, so usually accessible
		public static const usersKeyPath:String = "HKEY_USERS"; //All users on the machine. Usually need administrator privileges.
		public static const localMachineKeyPath:String = "HKEY_LOCAL_MACHINE"; //Local machine settings. May (?) need administrator privileges.
		public static const rootClassesKeyPath:String = "HKEY_CLASSES_ROOT";  //Windows service providers (?). May (?) need administrator privileges.
		public static const rootCurrentConfigKeyPath:String = "HKEY_CURRENT_CONFIG"; //Current system configuration. May (?) need administrator privileges.
		
		//Mappings / aliases; Windows Registry shorcuts (are there any other ones?)
		private static const aliases:Object = {
			"HKEY_CURRENT_USER":"HKCU", 
			"HKEY_USERS":"HKU",
			"HKEY_LOCAL_MACHINE":"HKLM",
			"HKEY_CLASSES_ROOT":"HKCR",
			"HKEY_CURRENT_CONFIG":"HKCC"
		};
		
		//Root paths below denote which keys need what type of access for what kind of operation.
		//Don't include any aliases, just the main key paths defined above.
		//The following require admin access to read from the registry:
		private static const requiresAdminReadAccess:Array = [
		];
		//...and the following require admin access to update/write to the registry:
		private static const requiresAdminUpdateAccess:Array = [
			usersKeyPath, localMachineKeyPath, rootClassesKeyPath, rootCurrentConfigKeyPath
		];
		
		/**
		 * Evaluates the supplied key path to determine if it probably requires administrator
		 * privileges to read data. This is based on default registry permissions and may
		 * not be completely accurate as permissions may have been updated.
		 * 
		 * @param	keyPath The key path, either full, partial, or using an alias, to evaluate.
		 * 
		 * @return True if the key path will probably need administrator privileges in order to
		 * read data, false otherwise.
		 */
		public static function keyPathRequiresReadAdminRights(keyPath:String):Boolean {
			if ((keyPath == null) || (keyPath == "")) {
				return (false);
			}//if
			try {
				var rootKey:String = keyPath.split("\\")[0] as String;
				var accessTypeArray:Array = requiresAdminReadAccess;				
				for (var count:uint = 0; count < accessTypeArray.length; count++) {
					var currentPath:String = accessTypeArray[count] as String;
					var currentAlias:String = getKeyPathAlias(currentPath);
					if ((currentPath == rootKey) || (currentAlias == rootKey)) {
						return (true);
					}//if
				}//for
			} catch (err:*) {
				return (false);
			}//catch
			return (false);
		}//keyPathRequiresReadAdminRights
		
		/**
		 * Evaluates the supplied key path to determine if it probably requires administrator
		 * privileges to update data.
		 * 
		 * @param	keyPath The key path, either full, partial, or using an alias, to evaluate.		 
		 * 
		 * @return True if the key path will probably need administrator privileges in order to
		 * update the Windows Registry, false otherwise.
		 */
		public static function keyPathRequiresUpdateAdminRights(keyPath:String):Boolean {
			if ((keyPath == null) || (keyPath == "")) {
				return (false);
			}//if
			try {
				var rootKey:String = keyPath.split("\\")[0] as String;
				var accessTypeArray:Array = requiresAdminUpdateAccess;
				for (var count:uint = 0; count < accessTypeArray.length; count++) {
					var currentPath:String = accessTypeArray[count] as String;
					var currentAlias:String = getKeyPathAlias(currentPath);
					if ((currentPath == rootKey) || (currentAlias == rootKey)) {
						return (true);
					}//if
				}//for
			} catch (err:*) {
				return (false);
			}//catch
			return (false);
		}//keyPathRequiresUpdateAdminRights
		
		/**
		 * Returns the key path alias (short form) of the specified key path. For example,
		 * "HKEY_CURRENT_USER\Network" would become "HKCU\Network". If  no alias exists,
		 * the path is simply returned as-is.
		 * 
		 * @param	keyPath The key path for which to attempt to find an alias for.
		 * @param	returnRootOnly If true, only the alias of the root key path is returned, otherwise
		 * the whole path is returned with the original root key replaced with its alias (if available).
		 * 
		 * @return The key path alias, either root only or full depending on the parameter, of the specified 
		 * key. If no alias can be found for the supplied key path, the path is returned as-is.
		 */
		public static function getKeyPathAlias(keyPath:String, returnRootOnly:Boolean=false):String {
			if ((keyPath == null) || (keyPath == "")) {
				return (null);
			}//if
			var pathSplit:Array = keyPath.split("\\");
			var compareKey:String = pathSplit[0] as String;
			for (var item:* in aliases) {
				var currentPath:String = item;
				var currentAlias:String = aliases[currentPath];
				if (compareKey == currentPath) {
					if (!returnRootOnly) {
						for (var count:uint = 1; count < pathSplit.length; count++) {
							currentAlias += "\\" + pathSplit[count] as String;
						}//for
						return (currentAlias);
					}//if
				}//if
			}//for
			return (keyPath);
		}//getKeyPathAlias
		
	}//WindowsRegistryDefaults class
	
}//package