// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.ValueCallback;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public class ArchiveTest extends AndroidWebViewTestBase {

    private static final long TEST_TIMEOUT = 20000L;

    private static final String TEST_PAGE =
            "data:text/html;utf-8,<html><head></head><body>test</body></html>";

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private AwTestContainerView mTestContainerView;

    @Override
    protected void setUp() throws Exception {
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
    }

    private void doArchiveTest(final AwContents contents, final String path,
            final boolean autoName, String expectedPath) throws InterruptedException {
        if (expectedPath != null) {
            File file = new File(expectedPath);
            file.delete();
        }

        // Set up a handler to handle the completion callback
        final Semaphore s = new Semaphore(0);
        final AtomicReference<String> msgPath = new AtomicReference<String>();
        final ValueCallback<String> callback = new ValueCallback<String>() {
            @Override
            public void onReceiveValue(String path) {
                msgPath.set(path);
                s.release();
            }
        };

        // Generate MHTML and wait for completion
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                contents.saveWebArchive(path, autoName, callback);
            }
        });
        assertTrue(s.tryAcquire(TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        assertEquals(expectedPath, msgPath.get());
        if (expectedPath != null) {
            File file = new File(expectedPath);
            assertTrue(file.exists());
            assertTrue(file.length() > 0);
        } else {
            // A path was provided, but the expected path was null. This means the save should have
            // failed, and so there shouldn't be a file path path.
            if (path != null) {
                assertFalse(new File(path).exists());
            }
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExplicitGoodPath() throws Throwable {
        final String path = new File(getActivity().getFilesDir(), "test.mht").getAbsolutePath();
        File file = new File(path);
        file.delete();
        assertFalse(file.exists());

        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, false, path);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutoGoodPath() throws Throwable {
        final String path = getActivity().getFilesDir().getAbsolutePath() + "/";

        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        // Create the first archive
        {
            String expectedPath = path + "index.mht";
            doArchiveTest(mTestContainerView.getAwContents(), path, true, expectedPath);
        }

        // Create a second archive, making sure that the second archive's name is auto incremented.
        {
            String expectedPath = path + "index-1.mht";
            doArchiveTest(mTestContainerView.getAwContents(), path, true, expectedPath);
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testExplicitBadPath() throws Throwable {
        final String path = new File("/foo/bar/baz.mht").getAbsolutePath();
        File file = new File(path);
        file.delete();
        assertFalse(file.exists());

        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, false, null);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutoBadPath() throws Throwable {
        final String path = new File("/foo/bar/").getAbsolutePath();
        File file = new File(path);
        file.delete();
        assertFalse(file.exists());

        loadUrlSync(mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), TEST_PAGE);

        doArchiveTest(mTestContainerView.getAwContents(), path, true, null);
    }

}
