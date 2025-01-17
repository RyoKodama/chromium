// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.annotation.IntDef;
import android.support.annotation.LayoutRes;
import android.view.View;
import android.widget.Button;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.ImpressionTracker;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Item that allows the user to perform an action on the NTP. Depending on its state, it can also
 * show a progress indicator over the same space. See {@link State}.
 */
public class ActionItem extends OptionalLeaf {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.HIDDEN, State.BUTTON, State.LOADING})
    public @interface State {
        int HIDDEN = 0;
        int BUTTON = 1;
        int LOADING = 2;
    }

    private final SuggestionsCategoryInfo mCategoryInfo;
    private final SuggestionsSection mParentSection;
    private final SuggestionsRanker mSuggestionsRanker;

    private boolean mImpressionTracked;
    private int mPerSectionRank = -1;
    private @State int mState = State.HIDDEN;

    public ActionItem(SuggestionsSection section, SuggestionsRanker ranker) {
        mCategoryInfo = section.getCategoryInfo();
        mParentSection = section;
        mSuggestionsRanker = ranker;
        updateState(State.BUTTON); // Also updates the visibility of the item.
    }

    @Override
    public int getItemViewType() {
        return ItemViewType.ACTION;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        mSuggestionsRanker.rankActionItem(this, mParentSection);
        ((ViewHolder) holder).onBindViewHolder(this);
    }

    @Override
    public void visitOptionalItem(NodeVisitor visitor) {
        switch (mState) {
            case State.BUTTON:
                visitor.visitActionItem(mCategoryInfo.getAdditionalAction());
                break;
            case State.LOADING:
                visitor.visitProgressItem();
                break;
            case State.HIDDEN:
                // If state is HIDDEN, itemCount should be 0 and this method should not be called.
            default:
                throw new IllegalStateException();
        }
    }

    @CategoryInt
    public int getCategory() {
        return mCategoryInfo.getCategory();
    }

    public void setPerSectionRank(int perSectionRank) {
        mPerSectionRank = perSectionRank;
    }

    public int getPerSectionRank() {
        return mPerSectionRank;
    }

    public void updateState(@State int newState) {
        if (newState == State.BUTTON
                && mCategoryInfo.getAdditionalAction() == ContentSuggestionsAdditionalAction.NONE) {
            newState = State.HIDDEN;
        }

        if (mState == newState) return;
        mState = newState;

        boolean newVisibility = (newState != State.HIDDEN);
        if (isVisible() != newVisibility) {
            setVisibilityInternal(newVisibility);
        } else {
            notifyItemChanged(0, (viewHolder) -> ((ViewHolder) viewHolder).setState(mState));
        }
    }

    public @State int getState() {
        return mState;
    }

    @VisibleForTesting
    void performAction(SuggestionsUiDelegate uiDelegate) {
        assert mState == State.BUTTON;

        uiDelegate.getEventReporter().onMoreButtonClicked(this);

        switch (mCategoryInfo.getAdditionalAction()) {
            case ContentSuggestionsAdditionalAction.VIEW_ALL:
                // The action does not reach the backend, so we record it here.
                SuggestionsMetrics.recordActionViewAll();
                mCategoryInfo.performViewAllAction(uiDelegate.getNavigationDelegate());
                return;
            case ContentSuggestionsAdditionalAction.FETCH:
                mParentSection.fetchSuggestions();
                return;
            case ContentSuggestionsAdditionalAction.NONE:
            default:
                // Should never be reached.
                assert false;
        }
    }

    /** ViewHolder associated to {@link ItemViewType#ACTION}. */
    public static class ViewHolder extends CardViewHolder implements ContextMenuManager.Delegate {
        private ActionItem mActionListItem;
        private final ProgressIndicatorView mProgressIndicator;
        private final Button mButton;

        public ViewHolder(SuggestionsRecyclerView recyclerView,
                ContextMenuManager contextMenuManager, SuggestionsUiDelegate uiDelegate,
                UiConfig uiConfig) {
            super(getLayout(), recyclerView, uiConfig, contextMenuManager);

            mProgressIndicator = itemView.findViewById(R.id.progress_indicator);
            mButton = itemView.findViewById(R.id.action_button);
            mButton.setOnClickListener(v -> mActionListItem.performAction(uiDelegate));

            new ImpressionTracker(itemView, () -> {
                if (mActionListItem != null && !mActionListItem.mImpressionTracked) {
                    mActionListItem.mImpressionTracked = true;
                    uiDelegate.getEventReporter().onMoreButtonShown(mActionListItem);
                }
            });
        }

        public void onBindViewHolder(ActionItem item) {
            super.onBindViewHolder();
            mActionListItem = item;
            setState(item.mState);
        }

        @LayoutRes
        private static int getLayout() {
            return FeatureUtilities.isChromeHomeEnabled()
                    ? R.layout.content_suggestions_action_card_modern
                    : R.layout.new_tab_page_action_card;
        }

        private void setState(@State int state) {
            assert state != State.HIDDEN;

            // When hiding children, we keep them invisible rather than GONE to make sure the
            // overall height of view does not change, to make transitions look better.
            if (state == State.BUTTON) {
                mButton.setVisibility(View.VISIBLE);
                mProgressIndicator.hide(/* keepSpace = */ true);
            } else if (state == State.LOADING) {
                mButton.setVisibility(View.INVISIBLE);
                mProgressIndicator.show();
            } else {
                // Not even HIDDEN is supported as the item should not be able to receive updates.
                assert false : "ActionViewHolder got notified of an unsupported state: " + state;
            }
        }
    }
}
