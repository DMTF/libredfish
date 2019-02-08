//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
public class RedfishPayload {
   static {
      System.loadLibrary("redfish");
   }

   private long cPayload;

   public RedfishPayload(long cPayload) {
       this.cPayload = cPayload;
   }

   public RedfishPayload(RedfishService service, String content) {
       this.cPayload = service.createRedfishPayloadFromString(content);
   }

   protected void finalize( ) throws Throwable {
       this.cleanupPayload(this.cPayload);
       this.cPayload = 0;
       super.finalize();
   }

   private native boolean isPayloadCollection(long payload);
   private native String getPayloadStringValue(long payload);
   private native long getPayloadIntValue(long payload);
   private native long getPayloadByNodeName(long payload, String nodeName);
   private native long getPayloadByIndex(long payload, long index);
   private native long getPayloadForPathString(long payload, String path);
   private native long cGetCollectionSize(long payload);
   private native long patchPayloadStringProperty(long payload, String propertyName, String value);
   private native long postPayload(long target, long payload);
   private native boolean deletePayload(long payload);
   private native String payloadToString(long payload, boolean prettyPrint);
   private native void cleanupPayload(long payload);
}
