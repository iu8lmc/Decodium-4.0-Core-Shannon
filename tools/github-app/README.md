# decodium-agent — GitHub App

**Registered.** <https://github.com/apps/decodium-agent>

| | |
|---|---|
| App ID | `4558437` |
| Client ID | `Iv23liO0CgAfiPmmkgrE` |
| Owner | `iu8lmc` |
| Installable by | any account (public) |

Both identifiers above are public — they appear on the app's own page and are
meant to be quoted in configuration. What must **never** be committed, pasted
into a chat, or shared is the **private key** (`.pem`) and the **client
secret**: those are the credentials that let something act *as* the app.

Registration material for a project assistant modelled on
[`aethersdr-agent`](https://github.com/apps/aethersdr-agent): triage of issues,
code review against the real sources, and pull requests opened for human review.

## Why this is not a script

There is no API or `gh` command that registers a GitHub App. The only
automatable route is the **app manifest flow**: a pre-filled manifest is handed
to GitHub and a human presses *Create GitHub App*. That is by design — an App is
an identity on your account.

## Steps

1. Open `create-decodium-agent.html` in a browser.
2. Leave the organisation field empty for a personal app, or type the
   organisation name.
3. Press the button. GitHub opens with the form already filled in. **Nothing is
   created until you press *Create GitHub App* on GitHub.**
4. On the app page afterwards:
   - note the **App ID**;
   - **Generate a private key** and store the `.pem` somewhere safe — GitHub
     shows it once;
   - **Install App** on the repositories it should act on.

`decodium-agent-manifest.json` holds the same manifest for reference or for a
scripted flow with a callback server.

`redirect_url` is **mandatory** — GitHub rejects the manifest without it
("Invalid GitHub App configuration … `redirect_url` wasn't supplied"), even
when no automatic credential exchange is wanted. It points at
`https://github.com/settings/apps`, so creation lands on the list of your apps,
which is where the private key is generated. GitHub appends a one-time `code`
to that URL; ignoring it is harmless — it only exists for the API call that
would hand back the App ID, private key and webhook secret automatically.

## If the manifest route misbehaves

The manifest flow is a convenience, not the only way. The app can be registered
by hand at <https://github.com/settings/apps/new>, which is deterministic:

| Field | Value |
|---|---|
| GitHub App name | `decodium-agent` (must be unique across all of GitHub) |
| Homepage URL | `https://github.com/iu8lmc/Decodium-4.0-Core-Shannon` |
| Webhook → Active | **unticked** |
| Webhook URL | `https://groups.ft2.it/decodium-agent/webhook` (only if Active is ticked) |
| Repository permissions | Issues: *Read and write* · Pull requests: *Read and write* · Contents: *Read and write* · Checks: *Read-only* · Actions: *Read-only* |
| Workflows | leave at *No access* — this is what stops the app touching CI |
| Subscribe to events | Issues, Issue comment, Pull request, Pull request review, Pull request review comment |
| Where can this be installed | *Any account* for a public app, *Only on this account* for a private one |

Metadata read-only is granted automatically and cannot be removed.

Note that the app is created the moment *Create GitHub App* is pressed — the
manifest flow's third step only retrieves credentials. So if the app does not
appear in <https://github.com/settings/apps>, the creation itself never
happened; there is nothing to clean up before retrying.

## What it grants

| Permission | Level | Why |
|---|---|---|
| `metadata` | read | mandatory for every app |
| `issues` | write | comment on and label triaged issues |
| `pull_requests` | write | open pull requests and review comments |
| `contents` | write | push the branch a pull request is built from |
| `checks` | read | read CI results to reason about failures |
| `actions` | read | read workflow runs and logs |

`workflows` is deliberately **not** granted, so the app cannot alter the CI
pipelines — the same limitation `aethersdr-agent` states.

The app is created **public**: it is listed at `github.com/apps/decodium-agent`
and anyone may install it on their own repositories. An installation grants
those permissions on *their* repositories only, and nothing happens unless your
backend chooses to act on the events it receives. To restrict it later, set the
app back to private on its settings page.

## Important

The App is only an identity and a permission set. **On its own it does
nothing.** Something has to run somewhere, authenticate as the app with the
private key, receive events and act. Until that backend exists, keep the webhook
disabled — as the manifest does.

### Before enabling the webhook

`groups.ft2.it` currently answers with a redirect, and it drops the path:

```
POST https://groups.ft2.it/decodium-agent/webhook
  → 301 https://community.ft2.it/groups
```

GitHub does **not** follow redirects when delivering a webhook; it records the
3xx as a failed delivery. So the host has to serve `/decodium-agent/webhook`
directly — on `groups.ft2.it` itself, or by pointing the manifest at
`community.ft2.it`, which answers on that path without redirecting (403 today,
because nothing is listening there yet). This only matters the day the webhook
is switched on.
