
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# Change USER if your Windows username differs.
SOURCE_DIR = Path(
	r"C:\Users\USER\Desktop\Новая папка\Dump\FBWGPS3-main\FBWGPS3_handoff_bundle"
)

REPO_URL = "https://github.com/OmegaSigmaDelta/FBWGPS3.git"
BRANCH = "main"
DESTINATION = "FBWGPS3_handoff_bundle"


def run_git(args, cwd=None):
	result = subprocess.run(
		["git", *args],
		cwd=cwd,
		text=True,
		encoding="utf-8",
		errors="replace"
	)

	if result.returncode != 0:
		raise RuntimeError(
			f"Git command failed: {' '.join(args)}"
		)


def copy_files(source, destination):
	if not source.is_dir():
		raise FileNotFoundError(
			f"Source folder not found: {source}"
		)

	destination.mkdir(parents=True, exist_ok=True)

	for item in source.iterdir():
		if item.name == ".git":
			continue

		target = destination / item.name

		if item.is_dir():
			shutil.copytree(
				item,
				target,
				dirs_exist_ok=True,
				ignore=shutil.ignore_patterns(
					".git",
					"__pycache__",
					".pytest_cache",
					"CMakeFiles"
				)
			)
		else:
			shutil.copy2(item, target)


def main():
	if shutil.which("git") is None:
		print("ERROR: Git is not installed or not on PATH.")
		return 1

	if not SOURCE_DIR.is_dir():
		print(f"ERROR: Folder not found:\n{SOURCE_DIR}")
		print("Edit SOURCE_DIR at the top of this script.")
		return 1

	try:
		with tempfile.TemporaryDirectory(
			prefix="fbwgps3_upload_"
		) as temp:
			repo = Path(temp) / "repo"

			print("Cloning repository...")
			run_git([
				"clone",
				"--branch", BRANCH,
				REPO_URL,
				str(repo)
			])

			destination = repo / DESTINATION
			print("Copying handoff bundle...")
			copy_files(SOURCE_DIR, destination)

			run_git(
				["add", "--all", DESTINATION],
				cwd=repo
			)

			status = subprocess.run(
				["git", "status", "--short"],
				cwd=repo,
				capture_output=True,
				text=True,
				encoding="utf-8",
				errors="replace"
			)

			if not status.stdout.strip():
				print("Nothing changed; repository is already up to date.")
				return 0

			print("\nChanges to upload:")
			print(status.stdout)

			answer = input(
				"\nType YES to commit and push these files: "
			).strip()

			if answer != "YES":
				print("Cancelled. Nothing was pushed.")
				return 0

			run_git([
				"-c", "user.name=FBWGPS3 Upload Script",
				"-c",
				"user.email=fbwgps3-upload@users.noreply.github.com",
				"commit",
				"-m", "Upload FBWGPS3 handoff bundle"
			], cwd=repo)

			print("Pushing to GitHub...")
			run_git(
				["push", "origin", BRANCH],
				cwd=repo
			)

			print("\nUpload successful!")
			print(
				"https://github.com/OmegaSigmaDelta/FBWGPS3"
			)
			print(f"Uploaded folder: {DESTINATION}/")
			return 0

	except Exception as error:
		print(f"\nERROR: {error}")
		print(
			"Check your GitHub access, internet connection, "
			"and Git installation."
		)
		return 1


if __name__ == "__main__":
	sys.exit(main())
