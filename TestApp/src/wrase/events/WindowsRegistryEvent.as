package wrase.events {
	
	import flash.events.Event;
	import wrase.model.WindowsRegistryKey;
	
	/**
	 * Event dispatched from WindowsRegistryEditor instances, typically for asynchronous operations.
	 * 
	 * @author Patrick Bay
	 */
	public class WindowsRegistryEvent extends Event {
		
		/**
		 * Dispatched when a registry key (and all child keys) has been successfully loaded.
		 */
		public static const ONLOAD:String = "Event.WindowsRegistryEvent.ONLOAD";
		/**
		 * Dispatched when an error was experienced during a registry key load.
		 */
		public static const ONLOADERROR:String = "Event.WindowsRegistryEvent.ONLOADERROR";
		/**
		 * Dispatched when an error was experienced while parsing registry data (loaded or assigned).
		 */
		public static const ONPARSEERROR:String = "Event.WindowsRegistryEvent.ONPARSEERROR";
		/**
		 * Dispatched when the registry update process successfully completes (registry should be updated).
		 */
		public static const ONUPDATE:String = "Event.WindowsRegistryEvent.ONUPDATE";
		/**
		 * Dispatched when the registry update process fails.
		 */
		public static const ONUPDATEERROR:String = "Event.WindowsRegistryEvent.ONUPDATEERROR";
		
		/**
		 * A reference to the WindowsRegistryKey instance that was loaded and parsed as part of a load request (null
		 * if no load or the load failed). Because the Windows Registry model is not strictly coupled, subsequent 
		 * operations may supercede or alter any loaded key (it should not be a conisidered a root key, in other words).
		 */
		public var loadedKey:WindowsRegistryKey = null;
		
		public function WindowsRegistryEvent(type:String, bubbles:Boolean = false, cancelable:Boolean = false) {
			super(type, bubbles, cancelable);
		}//constructor
		
	}//WindowsRegistryEvent class
	
}//package