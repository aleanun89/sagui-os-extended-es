#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# gh-agentes.sh - el «gh» de GitHub con el token de los agentes (Issues y Discussions).
#
# El token está en .env_agents (variable GT), ignorado por git y con permisos 600.
# Este guion lo pasa a gh por GH_TOKEN sin mostrarlo nunca. Repositorio por defecto:
# el de origin (aleanun89/miniOS).
#
#   tools/gh-agentes.sh issue list
#   tools/gh-agentes.sh issue-crear "Título" nota.md [etiqueta,etiqueta]   (por la API REST: el token
#                       no tiene permiso de contenido, que «gh issue create» pide)
#   tools/gh-agentes.sh issue comment 12 --body "..."
#   tools/gh-agentes.sh discusion-listar
#   tools/gh-agentes.sh discusion-crear "Categoría" "Título" fichero.md
#   tools/gh-agentes.sh discusion-leer NÚMERO
#   tools/gh-agentes.sh discusion-editar NÚMERO fichero.md   (sustituye el cuerpo)
#   tools/gh-agentes.sh discusion-titulo NÚMERO "Título nuevo"
#   tools/gh-agentes.sh discusion-comentar NÚMERO fichero.md
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV="$RAIZ/.env_agents"
[ -f "$ENV" ] || { echo "gh-agentes: falta .env_agents (token de los agentes); pídeselo al mantenedor" >&2; exit 1; }
GH_TOKEN="$(sed -n 's/^GT=//p' "$ENV")"
[ -n "$GH_TOKEN" ] || { echo "gh-agentes: .env_agents no tiene GT=..." >&2; exit 1; }
export GH_TOKEN
REPO="${GH_REPO:-aleanun89/miniOS}"
DONO="${REPO%/*}"; NOME="${REPO#*/}"

case "${1:-}" in
    issue-crear)
        titulo="${2:?título}"; fich="${3:?fichero}"; etq="${4:-}"
        args=(-f "title=$titulo" -F "body=@$fich")
        IFS=, read -ra ls <<< "$etq"; for l in "${ls[@]}"; do [ -n "$l" ] && args+=(-f "labels[]=$l"); done
        gh api "repos/$REPO/issues" "${args[@]}" --jq '"#\(.number) \(.html_url)"' ;;
    discusion-listar)
        gh api graphql -f query="query{repository(owner:\"$DONO\",name:\"$NOME\"){discussions(first:30,orderBy:{field:UPDATED_AT,direction:DESC}){nodes{number title category{name} updatedAt}}}}" \
            --jq '.data.repository.discussions.nodes[] | "#\(.number) [\(.category.name)] \(.title) (\(.updatedAt))"' ;;
    discusion-leer)
        gh api graphql -F n="${2:?número}" -f query="query(\$n:Int!){repository(owner:\"$DONO\",name:\"$NOME\"){discussion(number:\$n){title body comments(first:50){nodes{author{login} body}}}}}" \
            --jq '.data.repository.discussion | "# \(.title)\n\n\(.body)\n" , (.comments.nodes[] | "--- \(.author.login):\n\(.body)\n")' ;;
    discusion-editar)
        n="${2:?número}"; cuerpo="$(cat "${3:?fichero}")"
        id=$(gh api graphql -F n="$n" -f query="query(\$n:Int!){repository(owner:\"$DONO\",name:\"$NOME\"){discussion(number:\$n){id}}}" --jq '.data.repository.discussion.id')
        gh api graphql -f i="$id" -f b="$cuerpo" \
            -f query='mutation($i:ID!,$b:String!){updateDiscussion(input:{discussionId:$i,body:$b}){discussion{number url}}}' \
            --jq '.data.updateDiscussion.discussion | "#\(.number) \(.url)"' ;;
    discusion-titulo)
        n="${2:?número}"; titulo="${3:?título}"
        id=$(gh api graphql -F n="$n" -f query="query(\$n:Int!){repository(owner:\"$DONO\",name:\"$NOME\"){discussion(number:\$n){id}}}" --jq '.data.repository.discussion.id')
        gh api graphql -f i="$id" -f t="$titulo" \
            -f query='mutation($i:ID!,$t:String!){updateDiscussion(input:{discussionId:$i,title:$t}){discussion{number title}}}' \
            --jq '.data.updateDiscussion.discussion | "#\(.number) \(.title)"' ;;
    discusion-comentar)
        n="${2:?número}"; cuerpo="$(cat "${3:?fichero}")"
        id=$(gh api graphql -F n="$n" -f query="query(\$n:Int!){repository(owner:\"$DONO\",name:\"$NOME\"){discussion(number:\$n){id}}}" --jq '.data.repository.discussion.id')
        gh api graphql -f i="$id" -f b="$cuerpo" \
            -f query='mutation($i:ID!,$b:String!){addDiscussionComment(input:{discussionId:$i,body:$b}){comment{url}}}' \
            --jq '.data.addDiscussionComment.comment.url' ;;
    discusion-crear)
        cat_nome="${2:?categoría}"; titulo="${3:?título}"; cuerpo="$(cat "${4:?fichero}")"
        ids=$(gh api graphql -f query="query{repository(owner:\"$DONO\",name:\"$NOME\"){id discussionCategories(first:20){nodes{id name}}}}")
        repo_id=$(echo "$ids" | python3 -c 'import json,sys; print(json.load(sys.stdin)["data"]["repository"]["id"])')
        cat_id=$(echo "$ids" | python3 -c 'import json,sys; c=[n["id"] for n in json.load(sys.stdin)["data"]["repository"]["discussionCategories"]["nodes"] if n["name"]==sys.argv[1]]; print(c[0] if c else "")' "$cat_nome")
        [ -n "$cat_id" ] || { echo "gh-agentes: no existe la categoría «$cat_nome» (¿Discussions activadas?)" >&2; exit 1; }
        gh api graphql -f r="$repo_id" -f c="$cat_id" -f t="$titulo" -f b="$cuerpo" \
            -f query='mutation($r:ID!,$c:ID!,$t:String!,$b:String!){createDiscussion(input:{repositoryId:$r,categoryId:$c,title:$t,body:$b}){discussion{number url}}}' \
            --jq '.data.createDiscussion.discussion | "#\(.number) \(.url)"' ;;
    *)
        exec gh "$@" -R "$REPO" ;;
esac
