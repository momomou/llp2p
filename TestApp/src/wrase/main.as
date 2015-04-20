/*
package {
	
	import WindowsRegistryEditor;
	import flash.display.Sprite;
	import events.WindowsRegistryEvent;
	import model.WindowsRegistryKey;
	
	/**
	 * A demo.
	 * 
	 * In WRASE, the Registry is managed in a hierarchy. At the top is the WindowsRegistryEditor which uses "regedit.exe"
	 * to read and update the Windows Registry. Once the data is retrieved into a .reg file, it's read and parsed into
	 * WindowsRegistryKey instances. 
	 * 
	 * A key, or WindowsRegistryKey instance, is identified by it's key path...the "HKEY_CURRENT_USER\\Identities" below, 
	 * for example. The WindowsRegistryKey then creates WindowsRegistryEntry instances, which are the entries, or data 
	 * items within the key. Each data item has a name, value, and type properties. 
	 * 
	 * The WindowsRegistryEntry attepts to manage the type based on the value in order to dynamically convert between
	 * native Flash data types and Windows Registry data types.
	 * 
	 */
	public class Main extends Sprite {
		
		public var regEdit:WindowsRegistryEditor;		
		
		public function Main():void {
			this.regEdit = new WindowsRegistryEditor();
			this.regEdit.addEventListener(WindowsRegistryEvent.ONLOAD, this.onLoadRegistry);
			this.regEdit.addEventListener(WindowsRegistryEvent.ONLOADERROR, this.onLoadRegistryError);
			this.regEdit.addEventListener(WindowsRegistryEvent.ONPARSEERROR, this.onParseRegistryError);
			//Don't forget to double-escape the slashes...
			this.regEdit.loadRegistry("HKEY_CURRENT_USER\\Identities", true);
		}
		
		private function onLoadRegistry(eventObj:WindowsRegistryEvent):void {			
			trace ("-- REGISTRY LOADED --");
			trace ("Key requested: " + eventObj.loadedKey.keyPath); // Should be: HKEY_CURRENT_USER\Identities
			trace (" --- ");
			trace (this.regEdit); //...every object in WRASE toString()'s to a formatted Windows Registry object...
			//trace  (eventObj.loadedKey); //...for example...
			//trace (eventObj.loadedKey.entries[0]); //...and so on...
			//trace (eventObj.loadedKey.entries[0].name); //Each WindowsRegistryEntry has three properties: a name...
			//trace (eventObj.loadedKey.entries[0].value); //...a value...
			//trace (eventObj.loadedKey.entries[0].type); //...and a type (which is based on the value).
			//eventObj.loadedKey.entries[0].value="All your base are belong to us." //An example of how to update registry data.
			//this.regEdit.updateRegistry(); //And commit the changes!
		}
		
		private function onLoadRegistryError(eventObj:WindowsRegistryEvent):void {			
			trace ("There was a problem loading the registry!");
		}
		
		private function onParseRegistryError(eventObj:WindowsRegistryEvent):void {			
			trace ("There was a problem parsing the registry information!");
		}
		
	}
	
}
*/