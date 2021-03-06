/**
 * @file
 */

/******************************************************************************
 * Copyright 2012, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
package org.alljoyn.bus.p2p.service;

import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.Context;

public class StartServiceReceiver extends BroadcastReceiver {

    public StartServiceReceiver() {
        super();
    }

    @Override
    public void onReceive(Context context, Intent intent) {

        if (intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            Intent serviceIntent = new Intent();
            serviceIntent.setAction("org.alljoyn.bus.p2p.service.helper");
            context.startService(serviceIntent);
        }
    }
}
