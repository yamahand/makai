以下を実行してください：
```bash
gh pr view $ARGUMENTS --json comments,reviews --jq '.comments[].body, .reviews[].body'
```

上記で取得したレビューコメントを全て対応し、
修正後に `gh pr comment $ARGUMENTS --body "レビュー対応しました"` を実行してください。