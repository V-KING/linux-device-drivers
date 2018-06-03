/* 2.  ����jni���� */

package com.andriod.server;
import andriod.os.ILedService;

/* LedService �������� */
public class LedService extends ILedService.Stub{

	private static final String TAG = "LedService";
	
	/* call native to access hardware */
	public int ledCtrl(int which , int state) throws andriod.os.RemoteException
	{
		return native_ledCtrl(which , state);
	}

	public LedService() {
		native_ledOpen();
	}
	
	public static native int native_ledOpen();
	public static native void native_ledClose();
	public static native int native_ledCtrl(int which, int state);
	
}


