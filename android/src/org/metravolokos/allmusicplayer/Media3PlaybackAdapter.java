package org.metravolokos.allmusicplayer;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.exoplayer.ExoPlayer;


public final class Media3PlaybackAdapter {
    public static final int PREPARING = 0;
    public static final int READY = 1;
    public static final int PLAYING = 2;
    public static final int PAUSED = 3;
    public static final int STOPPED = 4;
    public static final int FINISHED = 5;
    public static final int ERROR = 6;

    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static volatile ExoPlayer player;
    private static volatile int state = PREPARING;
    private static volatile long positionMs;
    private static volatile long durationMs;
    private static volatile String errorText = "";
    private static volatile String activeTrack = "";
    private static boolean stoppedByUser;

    private static final Runnable POSITION_POLL = new Runnable() {
        @Override
        public void run() {
            ExoPlayer current = player;
            if (current == null)
                return;
            long position = current.getCurrentPosition();
            long duration = current.getDuration();
            positionMs = Math.max(0, position);
            durationMs = duration == C.TIME_UNSET ? 0 : Math.max(0, duration);
            if (current.isPlaying())
                state = PLAYING;
            MAIN.postDelayed(this, 200);
        }
    };

    private Media3PlaybackAdapter() {}

    public static void initialize(Context context) {
        MAIN.post(() -> {
            if (player != null)
                return;
            try {
                Context appContext = context.getApplicationContext();
                player = new ExoPlayer.Builder(appContext).build();
                player.addListener(new Player.Listener() {
                    @Override
                    public void onPlaybackStateChanged(int playbackState) {
                        updatePlaybackState(playbackState);
                    }

                    @Override
                    public void onIsPlayingChanged(boolean isPlaying) {
                        if (isPlaying) {
                            stoppedByUser = false;
                            state = PLAYING;
                        } else if (player != null && player.getPlaybackState() == Player.STATE_READY
                                && state == PLAYING) {
                            state = PAUSED;
                        }
                    }

                    @Override
                    public void onPlayerError(PlaybackException exception) {
                        stoppedByUser = false;
                        state = ERROR;
                        String code = PlaybackException.getErrorCodeName(exception.errorCode);
                        errorText = code.contains("IO_FILE_NOT_FOUND")
                                ? "Не найден встроенный тестовый файл. Код Media3: " + code
                                : "Системный Android декодер не смог открыть трек. Код Media3: " + code;
                    }
                });
                state = READY;
                errorText = "";
                MAIN.removeCallbacks(POSITION_POLL);
                MAIN.post(POSITION_POLL);
            } catch (RuntimeException exception) {
                player = null;
                state = ERROR;
                errorText = "Не удалось создать Media3 ExoPlayer.";
            }
        });
    }

    public static void selectTrack(String assetPath, String title) {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null) {
                state = ERROR;
                errorText = "Media3 ExoPlayer ещё не готов.";
                return;
            }
            if (assetPath == null || !assetPath.startsWith("audio/") || assetPath.contains("..")) {
                state = ERROR;
                errorText = "Недопустимый путь встроенного тестового файла.";
                return;
            }

            boolean resume = current.getPlayWhenReady() || current.isPlaying();
            activeTrack = title == null ? "" : title;
            errorText = "";
            positionMs = 0;
            durationMs = 0;
            stoppedByUser = false;
            state = PREPARING;
            try {
                Uri uri = Uri.parse("asset:///" + assetPath);
                current.setMediaItem(MediaItem.fromUri(uri));
                current.setPlayWhenReady(resume);
                current.prepare();
            } catch (RuntimeException exception) {
                state = ERROR;
                errorText = "Не удалось выбрать локальный трек Media3.";
            }
        });
    }

    public static void play() {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null)
                return;
            errorText = "";
            stoppedByUser = false;
            if (current.getPlaybackState() == Player.STATE_IDLE && current.getMediaItemCount() > 0)
                current.prepare();
            if (current.getPlaybackState() == Player.STATE_ENDED)
                current.seekToDefaultPosition();
            current.play();
            state = current.getPlaybackState() == Player.STATE_READY ? PLAYING : PREPARING;
        });
    }

    public static void pause() {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null)
                return;
            current.pause();
            if (current.getPlaybackState() != Player.STATE_IDLE)
                state = PAUSED;
        });
    }

    public static void stop() {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null)
                return;
            stoppedByUser = true;
            current.stop();
            current.seekToDefaultPosition();
            positionMs = 0;
            state = STOPPED;
        });
    }

    public static void seekTo(long position) {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null || current.getMediaItemCount() == 0)
                return;
            long max = durationMs;
            long target = Math.max(0, max > 0 ? Math.min(position, max) : position);
            current.seekTo(target);
            positionMs = target;
        });
    }

    public static void release() {
        MAIN.post(() -> {
            MAIN.removeCallbacks(POSITION_POLL);
            ExoPlayer current = player;
            player = null;
            if (current != null)
                current.release();
            positionMs = 0;
            durationMs = 0;
            state = STOPPED;
        });
    }

    public static int stateCode() {
        return state;
    }

    public static long positionMs() {
        return positionMs;
    }

    public static long durationMs() {
        return durationMs;
    }

    public static String errorText() {
        return errorText;
    }

    private static void updatePlaybackState(int playbackState) {
        if (state == ERROR)
            return;
        if (playbackState == Player.STATE_BUFFERING) {
            state = PREPARING;
        } else if (playbackState == Player.STATE_READY) {
            if (player != null && player.isPlaying())
                state = PLAYING;
            else if (state != PAUSED && state != STOPPED)
                state = READY;
        } else if (playbackState == Player.STATE_ENDED) {
            state = FINISHED;
        } else if (playbackState == Player.STATE_IDLE) {
            state = stoppedByUser ? STOPPED : (activeTrack.isEmpty() ? READY : PREPARING);
        }
    }
}
