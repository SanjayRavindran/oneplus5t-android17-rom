// SPDX-License-Identifier: Apache-2.0

package com.oneplus5t.keyhandler;

import android.content.Context;
import android.media.AudioManager;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.Settings;
import android.util.Slog;
import android.view.InputDevice;
import android.view.KeyEvent;

import com.android.internal.os.DeviceKeyHandler;

/**
 * Handles the OnePlus 5T hardware 3-position Alert Slider (gpio-slider:
 * KEY_F1 = Top/Silent, KEY_F2 = Middle/Vibrate, KEY_F3 = Bottom/Normal).
 */
public class KeyHandler implements DeviceKeyHandler {
    private static final String TAG = "OnePlus5TKeyHandler";
    private static final int KEYCODE_SLIDER_TOP = 59;    /* KEY_F1 */
    private static final int KEYCODE_SLIDER_MIDDLE = 60; /* KEY_F2 */
    private static final int KEYCODE_SLIDER_BOTTOM = 61; /* KEY_F3 */

    private final Context mContext;
    private final AudioManager mAudioManager;
    private final Vibrator mVibrator;

    public KeyHandler(Context context) {
        mContext = context;
        mAudioManager = context.getSystemService(AudioManager.class);
        mVibrator = context.getSystemService(Vibrator.class);
    }

    private void vibrate(int effectId) {
        if (mVibrator != null && mVibrator.hasVibrator()) {
            mVibrator.vibrate(VibrationEffect.createPredefined(effectId));
        }
    }

    @Override
    public KeyEvent handleKeyEvent(KeyEvent event) {
        int scanCode = event.getScanCode();
        if (scanCode != KEYCODE_SLIDER_TOP &&
                scanCode != KEYCODE_SLIDER_MIDDLE &&
                scanCode != KEYCODE_SLIDER_BOTTOM) {
            return event;
        }

        InputDevice dev = event.getDevice();
        if (dev == null || !"Tri-state key".equals(dev.getName())) {
            return event;
        }

        if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0 &&
                Settings.Secure.getInt(mContext.getContentResolver(),
                        Settings.Secure.USER_SETUP_COMPLETE, 0) != 0 &&
                mAudioManager != null) {
            if (scanCode == KEYCODE_SLIDER_TOP) {
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_SILENT);
                Slog.i(TAG, "Alert Slider -> Silent");
            } else if (scanCode == KEYCODE_SLIDER_MIDDLE) {
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_VIBRATE);
                vibrate(VibrationEffect.EFFECT_CLICK);
                Slog.i(TAG, "Alert Slider -> Vibrate");
            } else if (scanCode == KEYCODE_SLIDER_BOTTOM) {
                mAudioManager.setRingerModeInternal(AudioManager.RINGER_MODE_NORMAL);
                vibrate(VibrationEffect.EFFECT_DOUBLE_CLICK);
                Slog.i(TAG, "Alert Slider -> Normal");
            }
        }
        return null;
    }
}
