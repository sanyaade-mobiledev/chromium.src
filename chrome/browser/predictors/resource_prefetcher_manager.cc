// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace predictors {

ResourcePrefetcherManager::ResourcePrefetcherManager(
    ResourcePrefetchPredictor* predictor,
    const ResourcePrefetchPredictorConfig& config,
    net::URLRequestContextGetter* context_getter)
    : predictor_(predictor),
      config_(config),
      context_getter_(context_getter) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(predictor_);
  CHECK(context_getter_);
}

ResourcePrefetcherManager::~ResourcePrefetcherManager() {
  DCHECK(prefetcher_map_.empty())
      << "Did not call ShutdownOnUIThread or ShutdownOnIOThread. "
         " Will leak Prefetcher pointers.";
}

void ResourcePrefetcherManager::ShutdownOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  predictor_ = NULL;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::ShutdownOnIOThread,
                 this));
}

void ResourcePrefetcherManager::ShutdownOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  STLDeleteContainerPairSecondPointers(prefetcher_map_.begin(),
                                       prefetcher_map_.end());
}

void ResourcePrefetcherManager::MaybeAddPrefetch(
    const NavigationID& navigation_id,
    scoped_ptr<ResourcePrefetcher::RequestVector> requests) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const GURL& main_frame_url = navigation_id.main_frame_url;
  PrefetcherMap::iterator prefetcher_it = prefetcher_map_.find(main_frame_url);
  if (prefetcher_it != prefetcher_map_.end())
    return;

  ResourcePrefetcher* prefetcher = new ResourcePrefetcher(
      this, config_, navigation_id, requests.Pass());
  prefetcher_map_.insert(std::make_pair(main_frame_url, prefetcher));
  prefetcher->Start();
}

void ResourcePrefetcherManager::MaybeRemovePrefetch(
    const NavigationID& navigation_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PrefetcherMap::iterator it = prefetcher_map_.find(
      navigation_id.main_frame_url);
  if (it != prefetcher_map_.end() &&
      it->second->navigation_id() == navigation_id) {
    it->second->Stop();
  }
}

void ResourcePrefetcherManager::ResourcePrefetcherFinished(
    ResourcePrefetcher* resource_prefetcher,
    ResourcePrefetcher::RequestVector* requests) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // |predictor_| can only be accessed from the UI thread.
  scoped_ptr<ResourcePrefetcher::RequestVector> requests_ptr(requests);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::ResourcePrefetcherFinishedOnUI,
                 this,
                 resource_prefetcher->navigation_id(),
                 base::Passed(&requests_ptr)));

  const GURL& main_frame_url =
      resource_prefetcher->navigation_id().main_frame_url;
  PrefetcherMap::iterator it = prefetcher_map_.find(main_frame_url);
  DCHECK(it != prefetcher_map_.end());
  delete it->second;
  prefetcher_map_.erase(it);
}

void ResourcePrefetcherManager::ResourcePrefetcherFinishedOnUI(
    const NavigationID& navigation_id,
    scoped_ptr<ResourcePrefetcher::RequestVector> requests) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |predictor_| may have been set to NULL if the predictor is shutting down.
  if (predictor_)
    predictor_->FinishedPrefetchForNavigation(navigation_id,
                                              requests.release());
}

net::URLRequestContext* ResourcePrefetcherManager::GetURLRequestContext() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  return context_getter_->GetURLRequestContext();
}

}  // namespace predictors
