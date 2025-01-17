// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.MaterialProgressBar;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.ui.UiUtils;

import java.util.Locale;

/**
 * The view for a downloaded item displayed in the Downloads list.
 */
public class DownloadItemView extends SelectableItemView<DownloadHistoryItemWrapper>
        implements ThumbnailProvider.ThumbnailRequest {
    private final int mMargin;
    private final int mIconBackgroundColor;
    private final int mIconBackgroundColorSelected;
    private final ColorStateList mIconForegroundColorList;
    private final ColorStateList mCheckedIconForegroundColorList;
    private final int mIconBackgroundResId;

    private DownloadHistoryItemWrapper mItem;
    private int mIconResId;
    private int mIconSize;
    private int mIconCornerRadius;
    private Bitmap mThumbnailBitmap;

    // Controls common to completed and in-progress downloads.
    private LinearLayout mLayoutContainer;

    // Controls for completed downloads.
    private View mLayoutCompleted;
    private TextView mFilenameCompletedView;
    private TextView mDescriptionView;

    // Controls for in-progress downloads.
    private View mLayoutInProgress;
    private TextView mFilenameInProgressView;
    private TextView mDownloadStatusView;
    private TextView mDownloadPercentageView;
    private MaterialProgressBar mProgressView;
    private TintedImageButton mPauseResumeButton;
    private View mCancelButton;

    /**
     * Constructor for inflating from XML.
     */
    public DownloadItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMargin = context.getResources().getDimensionPixelSize(R.dimen.list_item_default_margin);
        mIconBackgroundColor = DownloadUtils.getIconBackgroundColor(context);
        mIconBackgroundColorSelected =
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.google_grey_600);
        mIconSize = getResources().getDimensionPixelSize(R.dimen.list_item_start_icon_width);
        mIconCornerRadius =
                getResources().getDimensionPixelSize(R.dimen.list_item_start_icon_corner_radius);
        mCheckedIconForegroundColorList = DownloadUtils.getIconForegroundColorList(context);

        mIconBackgroundResId = R.drawable.list_item_icon_modern_bg;

        if (FeatureUtilities.isChromeHomeEnabled()) {
            mIconForegroundColorList = ApiCompatibilityUtils.getColorStateList(
                    context.getResources(), R.color.dark_mode_tint);
        } else {
            mIconForegroundColorList = DownloadUtils.getIconForegroundColorList(context);
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mProgressView = (MaterialProgressBar) findViewById(R.id.download_progress_view);

        mLayoutContainer = (LinearLayout) findViewById(R.id.layout_container);
        mLayoutCompleted = findViewById(R.id.completed_layout);
        mLayoutInProgress = findViewById(R.id.progress_layout);

        mFilenameCompletedView = (TextView) findViewById(R.id.filename_completed_view);
        mDescriptionView = (TextView) findViewById(R.id.description_view);

        mFilenameInProgressView = (TextView) findViewById(R.id.filename_progress_view);
        mDownloadStatusView = (TextView) findViewById(R.id.status_view);
        mDownloadPercentageView = (TextView) findViewById(R.id.percentage_view);

        mPauseResumeButton = (TintedImageButton) findViewById(R.id.pause_button);
        mCancelButton = findViewById(R.id.cancel_button);

        mPauseResumeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mItem.isPaused()) {
                    mItem.resume();
                } else if (!mItem.isComplete()) {
                    mItem.pause();
                }
            }
        });
        mCancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mItem.cancel();
            }
        });
    }

    @Override
    public String getFilePath() {
        return mItem == null ? null : mItem.getFilePath();
    }

    @Override
    public void onThumbnailRetrieved(String filePath, Bitmap thumbnail) {
        if (TextUtils.equals(getFilePath(), filePath) && thumbnail != null
                && thumbnail.getWidth() > 0 && thumbnail.getHeight() > 0) {
            assert !thumbnail.isRecycled();
            setThumbnailBitmap(thumbnail);
        }
    }

    @Override
    public int getIconSize() {
        return mIconSize;
    }

    /**
     * Initialize the DownloadItemView. Must be called before the item can respond to click events.
     *
     * @param provider The BackendProvider that allows interacting with the data backends.
     * @param item     The item represented by this DownloadItemView.
     */
    public void displayItem(BackendProvider provider, DownloadHistoryItemWrapper item) {
        mItem = item;
        setItem(item);

        // Cancel any previous thumbnail request for the previously displayed item.
        ThumbnailProvider thumbnailProvider = provider.getThumbnailProvider();
        thumbnailProvider.cancelRetrieval(this);

        int fileType = item.getFilterType();

        // Pick what icon to display for the item.
        mIconResId = DownloadUtils.getIconResId(fileType, DownloadUtils.ICON_SIZE_24_DP);

        // Request a thumbnail for the file to be sent to the ThumbnailCallback. This will happen
        // immediately if the thumbnail is cached or asynchronously if it has to be fetched from a
        // remote source.
        mThumbnailBitmap = null;
        if (fileType == DownloadFilter.FILTER_IMAGE && item.isComplete()) {
            thumbnailProvider.getThumbnail(this);
        } else {
            // TODO(dfalcantara): Get thumbnails for audio and video files when possible.
        }

        if (mThumbnailBitmap == null) updateIconView();

        Context context = mDescriptionView.getContext();
        mFilenameCompletedView.setText(item.getDisplayFileName());
        mFilenameInProgressView.setText(item.getDisplayFileName());

        String description = String.format(Locale.getDefault(), "%s - %s",
                Formatter.formatFileSize(context, item.getFileSize()), item.getDisplayHostname());
        mDescriptionView.setText(description);

        if (item.isComplete()) {
            showLayout(mLayoutCompleted);
        } else {
            showLayout(mLayoutInProgress);
            mDownloadStatusView.setText(item.getStatusString());

            Progress progress = item.getDownloadProgress();

            if (item.isPaused()) {
                mPauseResumeButton.setImageResource(R.drawable.ic_play_arrow_white_24dp);
                mPauseResumeButton.setContentDescription(
                        getContext().getString(R.string.download_notification_resume_button));
                mProgressView.setIndeterminate(false);
            } else {
                mPauseResumeButton.setImageResource(R.drawable.ic_pause_white_24dp);
                mPauseResumeButton.setContentDescription(
                        getContext().getString(R.string.download_notification_pause_button));
                mProgressView.setIndeterminate(progress.isIndeterminate());
            }

            if (!progress.isIndeterminate()) {
                mProgressView.setProgress(progress.getPercentage());
            }

            // Display the percentage downloaded in text form.
            // To avoid problems with RelativeLayout not knowing how to place views relative to
            // removed views in the hierarchy, this code instead makes the percentage View's width
            // to 0 by removing its text and eliminating the margin.
            if (progress.isIndeterminate()) {
                mDownloadPercentageView.setText(null);
                ApiCompatibilityUtils.setMarginEnd(
                        (MarginLayoutParams) mDownloadPercentageView.getLayoutParams(), 0);
            } else {
                mDownloadPercentageView.setText(
                        DownloadUtils.getPercentageString(progress.getPercentage()));
                ApiCompatibilityUtils.setMarginEnd(
                        (MarginLayoutParams) mDownloadPercentageView.getLayoutParams(), mMargin);
            }
        }

        setLongClickable(item.isComplete());
    }

    /**
     * @param thumbnail The Bitmap to use for the icon ImageView.
     */
    public void setThumbnailBitmap(Bitmap thumbnail) {
        mThumbnailBitmap = thumbnail;
        updateIconView();
    }

    @Override
    public void onClick() {
        if (mItem != null && mItem.isComplete()) mItem.open();
    }

    @Override
    public boolean onLongClick(View view) {
        if (mItem != null && mItem.isComplete()) {
            return super.onLongClick(view);
        } else {
            return true;
        }
    }

    @Override
    protected void updateIconView() {
        if (isChecked()) {
            if (FeatureUtilities.isChromeHomeEnabled()) {
                mIconView.setBackgroundResource(mIconBackgroundResId);
                mIconView.getBackground().setLevel(
                        getResources().getInteger(R.integer.list_item_level_selected));
            } else {
                mIconView.setBackgroundColor(mIconBackgroundColorSelected);
            }
            mIconView.setImageResource(R.drawable.ic_check_googblue_24dp);
            mIconView.setTint(mCheckedIconForegroundColorList);
        } else if (mThumbnailBitmap != null) {
            assert !mThumbnailBitmap.isRecycled();
            mIconView.setBackground(null);
            if (FeatureUtilities.isChromeHomeEnabled()) {
                RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                        getResources(),
                        Bitmap.createScaledBitmap(mThumbnailBitmap, mIconSize, mIconSize, false));
                roundedIcon.setCornerRadius(mIconCornerRadius);
                mIconView.setImageDrawable(roundedIcon);
            } else {
                mIconView.setImageBitmap(mThumbnailBitmap);
            }
            mIconView.setTint(null);
        } else {
            if (FeatureUtilities.isChromeHomeEnabled()) {
                mIconView.setBackgroundResource(mIconBackgroundResId);
                mIconView.getBackground().setLevel(
                        getResources().getInteger(R.integer.list_item_level_default));
            } else {
                mIconView.setBackgroundColor(mIconBackgroundColor);
            }
            mIconView.setImageResource(mIconResId);
            mIconView.setTint(mIconForegroundColorList);
        }
    }

    private void showLayout(View layoutToShow) {
        if (mLayoutCompleted != layoutToShow) UiUtils.removeViewFromParent(mLayoutCompleted);
        if (mLayoutInProgress != layoutToShow) UiUtils.removeViewFromParent(mLayoutInProgress);

        if (layoutToShow.getParent() == null) {
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT);
            params.weight = 1;
            mLayoutContainer.addView(layoutToShow, params);
        }
    }
}
