# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import os
import requests

GITHUB_API_URL = "https://api.github.com"
REPO = os.getenv("GITHUB_REPOSITORY")
PR_NUMBER = os.getenv("GITHUB_REF").split("/")[-2]
TOKEN = os.getenv("GITHUB_TOKEN")

headers = {
    "Authorization": f"token {TOKEN}",
    "Accept": "application/vnd.github.v3+json",
}


def get_pr_details():
    url = f"{GITHUB_API_URL}/repos/{REPO}/pulls/{PR_NUMBER}"
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    return response.json()


def get_pr_comments():
    url = f"{GITHUB_API_URL}/repos/{REPO}/issues/{PR_NUMBER}/comments"
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    return response.json()


def get_pr_review_comments():
    url = f"{GITHUB_API_URL}/repos/{REPO}/pulls/{PR_NUMBER}/comments"
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    return response.json()


def get_pr_files():
    url = f"{GITHUB_API_URL}/repos/{REPO}/pulls/{PR_NUMBER}/files"
    response = requests.get(url, headers=headers)
    response.raise_for_status()
    return response.json()


def check_for_unchecked_tasks(text):
    return "[ ]" in text


def main():
    pr_details = get_pr_details()
    pr_comments = get_pr_comments()
    pr_review_comments = get_pr_review_comments()
    pr_files = get_pr_files()

    unchecked_tasks_in_description = False
    unchecked_tasks_in_pr_comments = False
    unchecked_tasks_in_pr_review_comments = False

    # Check PR description
    if check_for_unchecked_tasks(pr_details.get("body", "")):
        unchecked_tasks_in_description = True

    # Check PR comments
    for comment in pr_comments:
        if check_for_unchecked_tasks(comment.get("body", "")):
            unchecked_tasks_in_pr_comments = True

    # Check PR review comments
    for comment in pr_review_comments:
        if check_for_unchecked_tasks(comment.get("body", "")):
            unchecked_tasks_pr_review_comments = True

    if unchecked_tasks_in_description:
        print("There are unfinished tasks in the PR description.")
        exit(1)
    elif unchecked_tasks_in_pr_comments:
        print("There are unfinished tasks in the PR comments.")
        exit(1)
    elif unchecked_tasks_in_pr_review_comments:
        print("There are unfinished tasks in the PR code review comments.")
        exit(1)
    else:
        print("No unchecked tasks found.")


if __name__ == "__main__":
    main()
