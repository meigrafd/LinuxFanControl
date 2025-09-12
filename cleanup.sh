python3 _cleanup_report.py

read -p "Press [Enter] to start Cleanup..."

xargs -a cleanup_candidates.txt -r git rm
git commit -m "repo: remove unreferenced/legacy files"
