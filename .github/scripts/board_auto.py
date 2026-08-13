#!/usr/bin/env python3
"""Automação do board: issue aberta/reaberta -> Backlog, fechada -> Concluido.
Uso: board_auto.py <opened|closed|reopened> <numero_issue>
Env: GH_TOKEN (PAT com escopo project), PROJECT_ID, FIELD_ID, BACKLOG, CONCLUIDO
"""
import json, os, subprocess, sys

ACTION = sys.argv[1]
NUM = int(sys.argv[2])
PROJECT_ID = os.environ["PROJECT_ID"]
FIELD_ID = os.environ["FIELD_ID"]
BACKLOG = os.environ["BACKLOG"]
CONCLUIDO = os.environ["CONCLUIDO"]
REPO = "TipGroup-inc/TipOS-staging"

def gh(query, **vars):
    args = ["gh", "api", "graphql", "-f", f"query={query}"]
    for k, v in vars.items():
        args += ["-f" if isinstance(v, str) else "-F", f"{k}={v}"]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"gh api falhou: {r.stderr[:500]}")
    return json.loads(r.stdout)

issue = gh("""query($n: Int!) {
  repository(owner: "TipGroup-inc", name: "TipOS-staging") {
    issue(number: $n) { id projectItems(first: 10) { nodes { id project { id } } } }
  }
}""", n=NUM)["data"]["repository"]["issue"]

item_id = next(
    (it["id"] for it in issue["projectItems"]["nodes"]
     if it["project"]["id"] == PROJECT_ID),
    None,
)

if item_id is None:
    added = gh("""mutation($p: ID!, $c: ID!) {
      addProjectV2ItemById(input: { projectId: $p, contentId: $c }) { item { id } }
    }""", p=PROJECT_ID, c=issue["id"])
    item_id = added["data"]["addProjectV2ItemById"]["item"]["id"]
    print(f"card criado no board: {item_id}")

value = CONCLUIDO if ACTION == "closed" else BACKLOG
gh("""mutation($p: ID!, $i: ID!, $f: ID!, $v: String!) {
  updateProjectV2ItemFieldValue(input: {
    projectId: $p, itemId: $i, fieldId: $f,
    value: { singleSelectOptionId: $v }
  }) { projectV2Item { id } }
}""", p=PROJECT_ID, i=item_id, f=FIELD_ID, v=value)
print(f"#{NUM} {ACTION} -> {'Concluido' if ACTION == 'closed' else 'Backlog'} (ok)")