//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------
public class RedfishService {
   static {
      System.loadLibrary("redfish");
   }

   private long cService;

   public RedfishService(String host) {
       RedfishAuth auth = new RedfishAuth();
       auth.authType = RedfishAuthType.NoAuth;
       this.cService = this.createServiceEnumerator(host, "", auth, 0);
   }

   public RedfishService(String host, String rootUri) {
       RedfishAuth auth = new RedfishAuth();
       auth.authType = RedfishAuthType.NoAuth;
       this.cService = this.createServiceEnumerator(host, rootUri, auth, 0);
   }

   public RedfishService(String host, String rootUri, RedfishAuth auth) {
       this.cService = this.createServiceEnumerator(host, rootUri, auth, 0);
   }

   public RedfishService(String host, String rootUri, RedfishAuth auth, int flags) {
       this.cService = this.createServiceEnumerator(host, rootUri, auth, flags);
   }

   protected void finalize( ) throws Throwable {
       this.cleanupServiceEnumerator(this.cService);
       this.cService = 0;
       super.finalize();
   }

   public RedfishPayload getServiceRoot() {
       return new RedfishPayload(this.getRedfishServiceRoot(this.cService, ""));
   }

   public RedfishPayload getServiceRoot(String version) {
       return new RedfishPayload(this.getRedfishServiceRoot(this.cService, version));
   }

   public RedfishPayload getPayloadByPath(String path) {
       return new RedfishPayload(this.cGetPayloadByPath(this.cService, path));
   }

   private native long createServiceEnumerator(String host, String rootUri, RedfishAuth auth, int flags);
   private native long getRedfishServiceRoot(long service, String version);
   private native long cGetPayloadByPath(long service, String path);
   private native void cleanupServiceEnumerator(long service);

   // Test Driver
   public static void main(String[] args) {
      new RedfishService("http://127.0.0.1");
   }
}
