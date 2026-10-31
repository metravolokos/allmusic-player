package org.metravolokos.allmusicplayer;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.datasource.DataSource;
import androidx.media3.datasource.DataSpec;
import androidx.media3.exoplayer.source.ProgressiveMediaSource;
import androidx.media3.exoplayer.ExoPlayer;

import java.io.IOException;
import java.util.Collections;
import java.util.Map;


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
    private static volatile long cloudHandle;
    private static boolean stoppedByUser;

    private static native long nativeSize(long handle);
    private static native boolean nativeSeek(long handle, long position);
    private static native int nativeRead(long handle, long position, byte[] output,
                                        int outputOffset, int length);
    private static native void nativeInterrupt(long handle);
    private static native void nativeRelease(long handle);

    private static final class CloudDataSourceFactory implements DataSource.Factory {
        private final long handle;

        CloudDataSourceFactory(long sourceHandle) {
            handle = sourceHandle;
        }

        @Override
        public DataSource createDataSource() {
            return new CloudDataSource(handle);
        }
    }

    private static final class CloudDataSource implements DataSource {
        private static final int BRIDGE_READ_LIMIT = 64 * 1024;
        private final long handle;
        private Uri uri;
        private long position;
        private long resourceSize;
        private long bytesRemaining;
        private boolean opened;

        CloudDataSource(long sourceHandle) {
            handle = sourceHandle;
        }

        @Override
        public long open(DataSpec spec) throws IOException {
            close();
            if (!isOpaqueCloudUri(spec.uri, handle))
                throw new IOException("Cloud Mail source is invalid.");
            long size = nativeSize(handle);
            if (size <= 0 || spec.position < 0 || spec.position > size)
                throw new IOException("Cloud Mail source is unavailable.");
            if (!nativeSeek(handle, spec.position))
                throw new IOException("Cloud Mail source could not seek.");

            uri = spec.uri;
            position = spec.position;
            resourceSize = size;
            long available = size - position;
            bytesRemaining = spec.length == C.LENGTH_UNSET
                    ? available : Math.min(spec.length, available);
            opened = true;
            return bytesRemaining;
        }

        @Override
        public int read(byte[] buffer, int offset, int length) throws IOException {
            if (!opened)
                throw new IOException("Cloud Mail source is closed.");
            if (length == 0)
                return 0;
            if (bytesRemaining == 0 || position >= resourceSize)
                return C.RESULT_END_OF_INPUT;

            int count = (int) Math.min(Math.min((long) length, bytesRemaining),
                                       BRIDGE_READ_LIMIT);
            int read = nativeRead(handle, position, buffer, offset, count);
            if (read < 0) {
                if (position >= resourceSize || bytesRemaining == 0)
                    return C.RESULT_END_OF_INPUT;
                throw new IOException("Cloud Mail range request failed.");
            }
            if (read == 0)
                throw new IOException("Cloud Mail range request returned no data.");
            position += read;
            bytesRemaining -= read;
            return read;
        }

        @Override
        public Uri getUri() {
            return uri;
        }

        @Override
        public Map<String, java.util.List<String>> getResponseHeaders() {
            return Collections.emptyMap();
        }

        @Override
        public void addTransferListener(
                androidx.media3.datasource.TransferListener transferListener) {
            // Transfer callbacks do not expose remote names or credentials.
        }

        @Override
        public void close() {
            if (opened)
                nativeInterrupt(handle);
            opened = false;
            uri = null;
            bytesRemaining = 0;
        }

        private static boolean isOpaqueCloudUri(Uri uri, long handle) {
            return uri != null && "allmusic".equals(uri.getScheme())
                    && "stream".equals(uri.getAuthority())
                    && ("/" + Long.toUnsignedString(handle)).equals(uri.getPath());
        }
    }

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
            cloudHandle = 0;
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

    public static void selectCloudTrack(long handle) {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (current == null || handle <= 0) {
                state = ERROR;
                errorText = "Cloud Mail source is unavailable.";
                return;
            }
            cloudHandle = handle;
            activeTrack = "Cloud Mail stream";
            errorText = "";
            positionMs = 0;
            durationMs = 0;
            stoppedByUser = false;
            state = PREPARING;
            try {
                Uri uri = Uri.parse("allmusic://stream/" + Long.toUnsignedString(handle));
                MediaItem item = MediaItem.fromUri(uri);
                ProgressiveMediaSource source = new ProgressiveMediaSource.Factory(
                        new CloudDataSourceFactory(handle)).createMediaSource(item);
                current.setMediaSource(source);
                current.prepare();
                current.play();
            } catch (RuntimeException exception) {
                state = ERROR;
                errorText = "Не удалось открыть поток Cloud Mail через Media3.";
            }
        });
    }

    public static void clearCloudTrack(long handle) {
        MAIN.post(() -> {
            if (handle <= 0 || cloudHandle != handle)
                return;
            nativeInterrupt(handle);
            ExoPlayer current = player;
            if (current != null) {
                current.stop();
                current.clearMediaItems();
            }
            cloudHandle = 0;
            activeTrack = "";
            positionMs = 0;
            durationMs = 0;
            state = STOPPED;
        });
    }

    public static void reportNativeError(String message) {
        MAIN.post(() -> {
            ExoPlayer current = player;
            if (cloudHandle > 0)
                nativeInterrupt(cloudHandle);
            if (current != null)
                current.stop();
            state = ERROR;
            errorText = message == null || message.isEmpty()
                    ? "Cloud Mail stream failed." : message;
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
            if (cloudHandle > 0)
                nativeInterrupt(cloudHandle);
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
            long handle = cloudHandle;
            cloudHandle = 0;
            if (handle > 0)
                nativeRelease(handle);
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
