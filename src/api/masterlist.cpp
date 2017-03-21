/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#include "api/masterlist.h"

#include <iomanip>
#include <sstream>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "loot/exception/file_access_error.h"
#include "loot/exception/git_state_error.h"
#include "api/game/game.h"
#include "api/helpers/git_helper.h"

using boost::format;
using std::string;

namespace fs = boost::filesystem;

namespace loot {
MasterlistInfo Masterlist::GetInfo(const boost::filesystem::path& path, bool shortID) {
  // Compare HEAD and working copy, and get revision info.
  GitHelper git;
  MasterlistInfo info;

  if (!fs::exists(path)) {
    BOOST_LOG_TRIVIAL(info) << "Unknown masterlist revision: No masterlist present.";
    throw FileAccessError(string("N/A: No masterlist present at ") + path.string());
  } else if (!git.IsRepository(path.parent_path())) {
    BOOST_LOG_TRIVIAL(info) << "Unknown masterlist revision: Git repository missing.";
    throw GitStateError(string("Unknown: \"") + path.parent_path().string() + "\" is not a Git repository.");
  }

  BOOST_LOG_TRIVIAL(debug) << "Existing repository found, attempting to open it.";
  git.Call(git_repository_open(&git.GetData().repo, path.parent_path().string().c_str()));

  //Need to get the HEAD object, because the individual file has a different SHA.
  BOOST_LOG_TRIVIAL(info) << "Getting the Git object for the tree at HEAD.";
  git.Call(git_revparse_single(&git.GetData().object, git.GetData().repo, "HEAD"));

  BOOST_LOG_TRIVIAL(trace) << "Generating hex string for Git object ID.";
  if (shortID) {
    git.Call(git_object_short_id(&git.GetData().buffer, git.GetData().object));
    info.revision_id = git.GetData().buffer.ptr;
  } else {
    char c_rev[GIT_OID_HEXSZ + 1];
    info.revision_id = git_oid_tostr(c_rev, GIT_OID_HEXSZ + 1, git_object_id(git.GetData().object));
  }

  BOOST_LOG_TRIVIAL(trace) << "Getting date for Git object.";
  const git_oid * oid = git_object_id(git.GetData().object);
  git.Call(git_commit_lookup(&git.GetData().commit, git.GetData().repo, oid));
  git_time_t time = git_commit_time(git.GetData().commit);

  std::ostringstream out;
  out << std::put_time(std::gmtime(&time), "%Y-%m-%d");
  info.revision_date = out.str();

  BOOST_LOG_TRIVIAL(trace) << "Diffing masterlist HEAD and working copy.";
  info.is_modified = GitHelper::IsFileDifferent(path.parent_path(), path.filename().string());

  return info;
}

bool Masterlist::IsLatest(const boost::filesystem::path& path,
                          const std::string& repoBranch) {
  if (repoBranch.empty())
    throw std::invalid_argument("Repository branch must not be empty.");

  GitHelper git;

  if (!git.IsRepository(path.parent_path())) {
    BOOST_LOG_TRIVIAL(info) << "Cannot get latest masterlist revision: Git repository missing.";
    throw GitStateError(string("Unknown: \"") + path.parent_path().string() + "\" is not a Git repository.");
  }

  BOOST_LOG_TRIVIAL(info) << "Attempting to open repository.";
  git.Call(git_repository_open(&git.GetData().repo, path.parent_path().string().c_str()));

  git.Fetch("origin");

  // Get the remote branch's commit ID.
  git_oid branchOid;
  git.Call(git_reference_name_to_id(&branchOid, git.GetData().repo, (string("refs/remotes/origin/") + repoBranch).c_str()));

  // Get HEAD's commit ID.
  git_oid headOid;
  git.Call(git_reference_name_to_id(&headOid, git.GetData().repo, "HEAD"));

  return memcmp(branchOid.id, headOid.id, 20) == 0;
}

bool Masterlist::Update(const boost::filesystem::path& path, const std::string& repoUrl, const std::string& repoBranch) {
  GitHelper git;
  fs::path repoPath = path.parent_path();
  string filename = path.filename().string();

  if (repoUrl.empty() || repoBranch.empty())
    throw std::invalid_argument("Repository URL and branch must not be empty.");

  // Initialise checkout options.
  BOOST_LOG_TRIVIAL(debug) << "Setting up checkout options.";
  char * paths = new char[filename.length() + 1];
  strcpy(paths, filename.c_str());
  git.GetData().checkout_options.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_DONT_REMOVE_EXISTING;
  git.GetData().checkout_options.paths.strings = &paths;
  git.GetData().checkout_options.paths.count = 1;

  // Initialise clone options.
  git.GetData().clone_options.checkout_opts = git.GetData().checkout_options;
  git.GetData().clone_options.bare = 0;
  git.GetData().clone_options.checkout_branch = repoBranch.c_str();

  // Now try to access the repository if it exists, or clone one if it doesn't.
  BOOST_LOG_TRIVIAL(trace) << "Attempting to open the Git repository at: " << repoPath;
  if (!git.IsRepository(repoPath))
    git.Clone(repoPath, repoUrl);
  else {
    // Repository exists: check settings are correct, then pull updates.

    // Open the repository.
    BOOST_LOG_TRIVIAL(info) << "Existing repository found, attempting to open it.";
    git.Call(git_repository_open(&git.GetData().repo, repoPath.string().c_str()));

    // Set the remote URL.
    BOOST_LOG_TRIVIAL(info) << "Using remote URL: " << repoUrl;
    git.Call(git_remote_set_url(git.GetData().repo, "origin", repoUrl.c_str()));

    // Now fetch updates from the remote.
    git.Fetch("origin");

    // Check that a local branch with the correct name exists.
    int ret = git_branch_lookup(&git.GetData().reference, git.GetData().repo, repoBranch.c_str(), GIT_BRANCH_LOCAL);
    if (ret == GIT_ENOTFOUND)
        // Branch doesn't exist. Create a new branch using the remote branch's latest commit.
      git.CheckoutNewBranch("origin", repoBranch);
    else {
        // The local branch exists. Need to merge the remote branch
        // into it.
      git.Call(ret);  // Handle other errors from preceding branch lookup.

      // Check if HEAD points to the desired branch and set it to if not.
      if (!git_branch_is_head(git.GetData().reference)) {
        BOOST_LOG_TRIVIAL(trace) << "Setting HEAD to follow branch: " << repoBranch;
        git.Call(git_repository_set_head(git.GetData().repo, (string("refs/heads/") + repoBranch).c_str()));
      }

      // Get remote branch reference.
      git.Call(git_branch_upstream(&git.GetData().reference2, git.GetData().reference));

      BOOST_LOG_TRIVIAL(trace) << "Checking HEAD and remote branch's mergeability.";
      git_merge_analysis_t analysis;
      git_merge_preference_t pref;
      git.Call(git_annotated_commit_from_ref(&git.GetData().annotated_commit, git.GetData().repo, git.GetData().reference2));
      git.Call(git_merge_analysis(&analysis, &pref, git.GetData().repo, (const git_annotated_commit **)&git.GetData().annotated_commit, 1));

      if ((analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) == 0 && (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) == 0) {
          // The local branch can't be easily merged. Best just to delete and recreate it.
        BOOST_LOG_TRIVIAL(trace) << "Local branch cannot be easily merged with remote branch.";

        BOOST_LOG_TRIVIAL(trace) << "Deleting the local branch.";
        git.Call(git_branch_delete(git.GetData().reference));

        // Need to free ref before calling git.CheckoutNewBranch()
        git_reference_free(git.GetData().reference);
        git.GetData().reference = nullptr;
        git_reference_free(git.GetData().reference2);
        git.GetData().reference2 = nullptr;

        git.CheckoutNewBranch("origin", repoBranch);
      } else {
          // Get remote branch commit ID.
        git.Call(git_reference_peel(&git.GetData().object, git.GetData().reference2, GIT_OBJ_COMMIT));
        const git_oid * remote_commit_id = git_object_id(git.GetData().object);

        git_object_free(git.GetData().object);
        git.GetData().object = nullptr;
        git_reference_free(git.GetData().reference2);
        git.GetData().reference2 = nullptr;

        bool updateBranchHead = true;
        if ((analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) != 0) {
            // No merge is required, but HEAD might be ahead of the remote branch. Check
            // to see if that's the case, and move HEAD back to match the remote branch
            // if so.
          BOOST_LOG_TRIVIAL(trace) << "Local branch is up-to-date with remote branch.";
          BOOST_LOG_TRIVIAL(trace) << "Checking to see if local and remote branch heads are equal.";

          // Get local branch commit ID.
          git.Call(git_reference_peel(&git.GetData().object, git.GetData().reference, GIT_OBJ_COMMIT));
          const git_oid * local_commit_id = git_object_id(git.GetData().object);

          git_object_free(git.GetData().object);
          git.GetData().object = nullptr;

          updateBranchHead = local_commit_id->id != remote_commit_id->id;

          // If the masterlist in
          // HEAD also matches the masterlist file, no further
          // action needs to be taken. Otherwise, a checkout
          // must be performed and the checked-out file parsed.
          if (!updateBranchHead) {
            BOOST_LOG_TRIVIAL(trace) << "Local and remote branch heads are equal.";
            if (!GitHelper::IsFileDifferent(repoPath, filename)) {
              BOOST_LOG_TRIVIAL(info) << "Local branch and masterlist file are already up to date.";
              return false;
            }
          } else
            BOOST_LOG_TRIVIAL(trace) << "Local branch heads is ahead of remote branch head.";
        } else
          BOOST_LOG_TRIVIAL(trace) << "Local branch can be fast-forwarded to remote branch.";

        if (updateBranchHead) {
            // The remote branch reference points to a particular
            // commit. Update the local branch reference to point
            // to the same commit.
          BOOST_LOG_TRIVIAL(trace) << "Syncing local branch head with remote branch head.";
          git.Call(git_reference_set_target(&git.GetData().reference2, git.GetData().reference, remote_commit_id, "Setting branch reference."));

          git_reference_free(git.GetData().reference2);
          git.GetData().reference2 = nullptr;
        }

        git_reference_free(git.GetData().reference);
        git.GetData().reference = nullptr;

        BOOST_LOG_TRIVIAL(trace) << "Performing a Git checkout of HEAD.";
        git.Call(git_checkout_head(git.GetData().repo, &git.GetData().checkout_options));
      }
    }
  }

  // Now whether the repository was cloned or updated, the working directory contains
  // the latest masterlist. Try parsing it: on failure, detach the HEAD back one commit
  // and try again.

  bool parsingFailed = false;
  do {
    // Get the HEAD revision's short ID.
    string revision = git.GetHeadShortId();

    //Now try parsing the masterlist.
    BOOST_LOG_TRIVIAL(debug) << "Testing masterlist parsing.";
    try {
      this->Load(path);

      parsingFailed = false;
    } catch (std::exception& e) {
      parsingFailed = true;

      //There was an error, roll back one revision.
      BOOST_LOG_TRIVIAL(error) << "Masterlist parsing failed. Masterlist revision " + string(revision) + ": " + e.what();
      git.CheckoutRevision("HEAD^");
    }
  } while (parsingFailed);

  return true;
}
}
