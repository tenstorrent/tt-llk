PR #90: docs: update README with bounty program information
URL: https://github.com/tenstorrent/tt-llk/pull/90

File: .github/workflows/pre-commit.yml
@@ -32,6 +32,7 @@ jobs:
   pr-comment:
     name: Post comment on PR
     runs-on: ubuntu-latest
+    if: github.event.pull_request.head.repo.full_name == github.repository
     steps:
       - name: Check if comment already exists
         id: check-comment

File: README.md
@@ -19,6 +19,8 @@ Test environment requires SFPI compiler for building, which is automatically ing
 4. Add new tests to cover your changes if needed and run existing ones.
 5. Start a pull request (PR).
 
+# Tenstorrent Bounty Program Terms and Conditions
+This repo is a part of Tenstorrent’s bounty program. If you are interested in helping to improve tt-llk, please make sure to read the [Tenstorrent Bounty Program Terms and Conditions](https://docs.tenstorrent.com/bounty_terms.html) before heading to the issues tab. Look for the issues that are tagged with both 'bounty' and difficulty level labels!
 
 ### Note
 Old LLK repositories (https://github.com/tenstorrent/tt-llk-gs, https://github.com/tenstorrent/tt-llk-wh-b0, https://github.com/tenstorrent/tt-llk-bh) are merged here and archived.

