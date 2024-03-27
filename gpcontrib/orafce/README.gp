# How to update repo by subtree

 1. Add the repo URL as a git remote.
```
git remote add -f orafce_remote https://github.com/orafce/orafce.git
```

 2. Clone the repo to a directory as a subtree.
```
git subtree add --prefix gpcontrib/orafce orafce_remote master --squash
```

 3. To update the subtree later:
```
git fetch orafce_remote master
git subtree pull --prefix gpcontrib/orafce orafce_remote master --squash
```

 4. Update subtree to a specific commit.
```
git subtree merge --prefix gpcontrib/orafce --squash commit_id
```
