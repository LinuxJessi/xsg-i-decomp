#!/usr/bin/env python3
"""Local SQLite work queue, Python >= 3.10, standard library only.

Use one shared absolute database path on a local filesystem, not NFS.
Resources are exact strings, not paths to resolve or glob/prefix locks.
`done` means the assignment was accepted, not that code matches a target.
`retired` means the assignment was superseded and will never be done; it is
recorded separately from `done` so no retirement can ever read as acceptance.
`ready_to_integrate` is a reviewer's recorded verdict on one submission; the
task stays `submitted` (locks retained) until it is closed after publication,
so readiness can never read as acceptance either. The publication step closes
it (`finish --closed-by-integration`, run by tools/tu_publish.py), recording
the marking reviewer as the accepting identity.
Owners/reviewers are caller-supplied identities, not authenticated users.
"""

import argparse
from contextlib import closing
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import signal
import sqlite3
import stat
import sys
import time


DEFAULT_LEASE = 1800
MAX_LEASE = 86400
STATUSES = ("pending", "claimed", "submitted", "done")
# `retired` is not a task status: a retired task keeps the status it had
# (always `pending`) and gains a row in `retired`, so nothing that reads the
# four statuses can mistake a retirement for an acceptance. It is accepted as a
# `list --status` selector only. `ready_to_integrate` is the same kind of
# selector: a `submitted` task whose current submission a reviewer marked ready.
LIST_STATUSES = STATUSES + ("retired", "ready_to_integrate")
VERSION = 2
SUBMITTED_REVISION_SCHEMA = """CREATE TABLE IF NOT EXISTS submitted_revisions (
    successor_task TEXT PRIMARY KEY NOT NULL REFERENCES tasks(id),
    predecessor_task TEXT NOT NULL REFERENCES tasks(id),
    predecessor_submission_id TEXT NOT NULL,
    predecessor_artifact TEXT NOT NULL,
    predecessor_artifact_sha256 TEXT NOT NULL,
    predecessor_owner TEXT NOT NULL,
    resource TEXT NOT NULL,
    binary_sha256 TEXT NOT NULL,
    unit TEXT NOT NULL,
    address TEXT NOT NULL,
    function_name TEXT NOT NULL,
    created_at REAL NOT NULL,
    actor TEXT NOT NULL,
    reason TEXT NOT NULL,
    UNIQUE(predecessor_task, predecessor_submission_id, resource)
)"""
SCHEMA = (
    """CREATE TABLE tasks (
        id TEXT PRIMARY KEY NOT NULL,
        title TEXT NOT NULL,
        priority INTEGER NOT NULL,
        status TEXT NOT NULL CHECK(status IN ('pending','claimed','submitted','done')),
        owner TEXT,
        token TEXT,
        lease_until REAL,
        artifact TEXT,
        created_at REAL NOT NULL,
        updated_at REAL NOT NULL
    )""",
    """CREATE TABLE resources (
        task_id TEXT NOT NULL REFERENCES tasks(id),
        resource TEXT NOT NULL,
        PRIMARY KEY(task_id, resource)
    )""",
    """CREATE TABLE dependencies (
        task_id TEXT NOT NULL REFERENCES tasks(id),
        depends_on TEXT NOT NULL REFERENCES tasks(id),
        PRIMARY KEY(task_id, depends_on)
    )""",
    """CREATE TABLE locks (
        resource TEXT PRIMARY KEY NOT NULL,
        task_id TEXT NOT NULL REFERENCES tasks(id)
    )""",
    """CREATE TABLE audit (
        id INTEGER PRIMARY KEY,
        at REAL NOT NULL,
        action TEXT NOT NULL,
        task_id TEXT REFERENCES tasks(id),
        actor TEXT,
        detail TEXT NOT NULL
    )""",
)
# Added in version 2, additively: the earlier tables are untouched, so an older
# database migrates by creating this one table and bumping the user version.
RETIRED_SCHEMA = (
    """CREATE TABLE retired (
        task_id TEXT PRIMARY KEY NOT NULL REFERENCES tasks(id),
        at REAL NOT NULL,
        actor TEXT NOT NULL,
        reason TEXT NOT NULL,
        superseded_by TEXT REFERENCES tasks(id)
    )""",
)
# Added 2026-09-16 without a version bump, on purpose: private snapshots of an
# older workqueue.py accept only user_version 1 or 2, and nothing they read
# changes, so bumping would break running agents for no gain. The table is
# created by `init` or lazily by the first `ready`, and every reader treats its
# absence as "no task is marked ready". One row per (task, submission): a
# resubmission is a new row, and closed rows stay as history.
READY_SCHEMA = (
    """CREATE TABLE IF NOT EXISTS integration_ready (
        task_id TEXT NOT NULL REFERENCES tasks(id),
        submission_id TEXT NOT NULL,
        at REAL NOT NULL,
        reviewer TEXT NOT NULL,
        report TEXT NOT NULL,
        report_sha256 TEXT NOT NULL,
        note TEXT NOT NULL,
        closed_at REAL,
        closed_by TEXT,
        closed_action TEXT,
        closed_reason TEXT,
        PRIMARY KEY(task_id, submission_id)
    )""",
)
# Read accelerators, added 2026-10-11 without a version bump for the same reason
# as READY_SCHEMA: they change no stored row and no query result, only how fast
# the tools find rows. The queue had one secondary index (audit.task_id), so a
# lookup of who holds `otu:<tu>` or a recent-activity query read the whole
# resources (1.1M rows) or audit (600k rows) table, which under a loaded host
# took minutes and held the write lock when it ran inside a claim. `connect`
# creates any missing one idempotently (CREATE INDEX IF NOT EXISTS) in its own
# short transaction, and skips it silently when the queue is busy. The NOCASE
# actor index serves `actor LIKE 'prefix%'` (case-insensitive LIKE needs it).
INDEXES = (
    ("resources_resource_idx", "CREATE INDEX IF NOT EXISTS resources_resource_idx "
                               "ON resources(resource)"),
    ("locks_task_id_idx", "CREATE INDEX IF NOT EXISTS locks_task_id_idx ON locks(task_id)"),
    ("tasks_status_idx", "CREATE INDEX IF NOT EXISTS tasks_status_idx ON tasks(status)"),
    ("dependencies_depends_on_idx", "CREATE INDEX IF NOT EXISTS dependencies_depends_on_idx "
                                    "ON dependencies(depends_on)"),
    ("audit_action_at_idx", "CREATE INDEX IF NOT EXISTS audit_action_at_idx ON audit(action, at)"),
    ("audit_actor_nocase_idx", "CREATE INDEX IF NOT EXISTS audit_actor_nocase_idx "
                               "ON audit(actor COLLATE NOCASE)"),
)
INDEX_TABLES = {"resources", "locks", "tasks", "dependencies", "audit"}
# How long `connect` waits for the write lock to add a missing index before it
# leaves the work to a later connection (milliseconds).
INDEX_BUSY_MS = 2000
# Queues larger than this get their indexes from `init`, not from any connection.
AUTO_INDEX_MAX_BYTES = 64 << 20
# Page-cache cap (KiB, negative pragma form) while building a missing index.
INDEX_CACHE_KIB = -(1 << 20)
# A TU-mode candidate holds a TU or TU-slice lock. The integrator's own task
# holds `baseline:integration`: its acceptance is the publication itself.
TU_CANDIDATE_PREFIXES = ("otu:", "otuslice:")
INTEGRATION_RESOURCE = "baseline:integration"
# Token files: shared temporary roots and session scratchpads are refused,
# because two agents of one session share them and clobber each other's claims.
SHARED_TEMP_ROOTS = ("/tmp", "/var/tmp", "/dev/shm")
SHARED_PATH_PARTS = ("scratchpad",)
HEARTBEAT_INTERVAL = 200
MAX_HEARTBEAT_INTERVAL = 240
DEFAULT_DAEMON_SECONDS = 7200
# A heartbeat that cannot reach the database keeps retrying with backoff until
# the lease it last renewed is this close to lapsing, then gives up loudly.
DAEMON_SAFETY_MARGIN = 60
BACKOFF = (2, 4, 8, 16, 30, 60)
# sqlite busy timeout of every connection (seconds; PRAGMA busy_timeout).
BUSY_TIMEOUT_SECONDS = 60


class QueueError(Exception):
    pass


class DaemonStop(Exception):
    """Ends the heartbeat daemon loop."""


class Parser(argparse.ArgumentParser):
    def error(self, message):
        raise QueueError(message)


def nonempty(value):
    if not value.strip():
        raise argparse.ArgumentTypeError("must not be empty or whitespace")
    return value


def bounded_seconds(low, high, what):
    def convert(value):
        try:
            seconds = int(value)
        except ValueError:
            raise argparse.ArgumentTypeError(f"{what} must be an integer") from None
        if not low <= seconds <= high:
            raise argparse.ArgumentTypeError(f"{what} must be between {low} and {high} seconds")
        return seconds
    return convert


lease_seconds = bounded_seconds(1, MAX_LEASE, "lease")


def priority(value):
    try:
        number = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("priority must be an integer") from None
    if not -(2**31) <= number < 2**31:
        raise argparse.ArgumentTypeError("priority must fit a signed 32-bit integer")
    return number


def token_arguments(command):
    """--token or --token-file, exactly one. The file keeps the token out of argv."""
    group = command.add_mutually_exclusive_group(required=True)
    group.add_argument("--token", type=nonempty)
    group.add_argument("--token-file", type=nonempty, dest="token_file",
                       help="private 0600 file holding the lease token (written by "
                            "`claim --token-file` or the dispatcher's lease-token)")


def parser():
    result = Parser(description=__doc__)
    result.add_argument("--db", required=True, help="shared absolute local database path")
    result.add_argument("--busy-retry", type=bounded_seconds(0, 600, "busy retry"), default=0,
                        dest="busy_retry",
                        help="retry a command refused with 'database is locked' for up to this "
                             "many seconds with backoff (default 0: fail at once). A locked "
                             "command committed nothing, so a retry is safe")
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("init", help="create the database, or verify an existing queue")
    add = commands.add_parser("add")
    add.add_argument("id", type=nonempty)
    add.add_argument("--title", required=True, type=nonempty)
    add.add_argument("--resource", action="append", required=True, type=nonempty)
    add.add_argument("--depends-on", action="append", default=[], type=nonempty)
    add.add_argument("--priority", type=priority, default=0)
    listing = commands.add_parser("list")
    listing.add_argument("--status", choices=LIST_STATUSES,
                         help="a task status, `retired` for the retired backlog, or "
                              "`ready_to_integrate` for submitted tasks a reviewer marked ready")
    listing.add_argument("--include-retired", action="store_true",
                         help="also list retired tasks (hidden by default, because they are "
                              "no longer work)")
    history = commands.add_parser("history", help="show task audit events, including prior reviews")
    history.add_argument("id", type=nonempty)
    claim = commands.add_parser("claim")
    claim.add_argument("--owner", required=True, type=nonempty)
    claim.add_argument("--task", type=nonempty)
    claim.add_argument("--lease-seconds", type=lease_seconds, default=DEFAULT_LEASE)
    claim.add_argument("--token-file", type=nonempty, dest="token_file",
                       help="write the lease token to this private path (mode 0600, atomic) "
                            "and print the path instead of the token")
    for name in ("heartbeat", "submit", "release"):
        command = commands.add_parser(name)
        command.add_argument("id", type=nonempty)
        command.add_argument("--owner", required=True, type=nonempty)
        token_arguments(command)
        if name == "heartbeat":
            command.add_argument("--lease-seconds", type=lease_seconds, default=DEFAULT_LEASE)
            mode = command.add_mutually_exclusive_group()
            mode.add_argument("--daemon", action="store_true",
                              help="fork a detached renewer (requires --token-file); it retries "
                                   "a locked database with backoff and stops at the stopfile, "
                                   "at --max-seconds, or when the claim ends")
            mode.add_argument("--stop", action="store_true",
                              help="stop the daemon started with this --token-file")
            command.add_argument("--interval", default=HEARTBEAT_INTERVAL,
                                 type=bounded_seconds(5, MAX_HEARTBEAT_INTERVAL, "interval"))
            command.add_argument("--max-seconds", default=DEFAULT_DAEMON_SECONDS, dest="max_seconds",
                                 type=bounded_seconds(60, MAX_LEASE, "max seconds"),
                                 help="daemon lifetime bound, so an abandoned daemon cannot keep "
                                      "a dead agent's claim alive forever")
            command.add_argument("--pidfile", type=nonempty,
                                 help="default: TOKEN_FILE.heartbeat.pid")
            command.add_argument("--stopfile", type=nonempty,
                                 help="default: TOKEN_FILE.heartbeat.stop")
            command.add_argument("--log", type=nonempty,
                                 help="default: TOKEN_FILE.heartbeat.log")
        elif name == "submit":
            command.add_argument("--artifact", required=True, type=nonempty)
        else:
            command.add_argument("--note", required=True, type=nonempty)
    narrow = commands.add_parser(
        "narrow",
        help="drop named resources from a live claim, keeping the rest; a worker that "
             "recovered part of its allocation releases the functions it is not delivering "
             "before it submits the exact subset")
    narrow.add_argument("id", type=nonempty)
    narrow.add_argument("--owner", required=True, type=nonempty)
    token_arguments(narrow)
    narrow.add_argument("--drop-resource", action="append", required=True, type=nonempty,
                        dest="drop_resource",
                        help="an exact resource string of this task (repeatable)")
    narrow.add_argument("--reason", required=True, type=nonempty,
                        help="why these resources are being released")
    repair = commands.add_parser(
        "narrow-submitted",
        help="repair a partial TU submission that retained undelivered function locks; "
             "preserves the immutable artifact, candidate locks and submitted status")
    repair.add_argument("id", type=nonempty)
    repair.add_argument("--owner", required=True, type=nonempty,
                        help="must equal the current submission owner")
    repair.add_argument("--operator", type=nonempty,
                        help="coordinator performing a cross-owner repair, recorded as audit actor; "
                             "--owner still pins the unchanged submission owner")
    repair.add_argument("--submission-id", required=True, type=nonempty,
                        help="must equal the current submitted task's submission ID")
    repair.add_argument("--artifact-sha256", required=True, type=nonempty,
                        help="pin the immutable submitted result.json before releasing locks")
    repair.add_argument("--analysis-only", action="store_true",
                        help="drop all held func: locks for an analysis_only submission "
                             "with zero exact candidates; retain the TU reservation")
    repair.add_argument("--rebase-slice-to", type=nonempty, metavar="VA",
                        help="move a stale otuslice: marker from an undelivered first function "
                             "to the first retained exact candidate's VA; no function lock moves")
    repair.add_argument("--drop-resource", action="append", type=nonempty,
                        dest="drop_resource", help="undelivered func: lock (repeatable)")
    repair.add_argument("--reason", required=True, type=nonempty)
    reclaim = commands.add_parser(
        "reclaim",
        help="lost-token recovery by the same owner: rotate the token of a claim this owner "
             "holds (live or lapsed but not yet swept), keeping its locks, and write the new "
             "token to --token-file; the old token stops working")
    reclaim.add_argument("id", type=nonempty)
    reclaim.add_argument("--owner", required=True, type=nonempty,
                         help="the current owner, which must match the claim")
    reclaim.add_argument("--token-file", required=True, type=nonempty, dest="token_file")
    reclaim.add_argument("--reason", required=True, type=nonempty)
    reclaim.add_argument("--lease-seconds", type=lease_seconds, default=DEFAULT_LEASE)
    revision = commands.add_parser(
        "revise-submitted", help="atomically claim an immutable submitted IOP singleton's "
        "successor; delegates its function lock without review or acceptance")
    revision.add_argument("id", type=nonempty, help="fresh successor task, or its pending retry")
    revision.add_argument("--predecessor", required=True, type=nonempty)
    revision.add_argument("--owner", required=True, type=nonempty)
    revision.add_argument("--submission-id", required=True, type=nonempty)
    revision.add_argument("--artifact-sha256", required=True, type=nonempty)
    revision.add_argument("--binary-sha256", required=True, type=nonempty)
    revision.add_argument("--unit", required=True, choices=("iop_ssd", "iop_rssd"))
    revision.add_argument("--address", required=True, type=nonempty)
    revision.add_argument("--function", required=True, type=nonempty)
    revision.add_argument("--reason", required=True, type=nonempty)
    revision.add_argument("--resource", action="append", default=[], type=nonempty,
                          help="fresh successor private file:/sandbox: reservation; repeatable")
    revision.add_argument("--lease-seconds", type=lease_seconds, default=DEFAULT_LEASE)
    revision.add_argument("--token-file", type=nonempty)
    retire = commands.add_parser(
        "retire",
        help="record that a pending assignment is superseded and will never be done; it "
             "leaves the backlog without ever being reported as accepted")
    retire.add_argument("id", type=nonempty)
    retire.add_argument("--owner", required=True, type=nonempty,
                        help="the identity retiring the task, recorded in the audit trail")
    retire.add_argument("--reason", required=True, type=nonempty,
                        help="the concrete evidence that the work landed, was never needed, "
                             "or never had content")
    retire.add_argument("--superseded-by", type=nonempty, dest="superseded_by",
                        help="the existing task that did this work instead")
    retire.add_argument("--published-supersession", action="store_true",
                        help="retire a pinned zero-exact IOP singleton after its exact replacement was published")
    for name in ("submission-id", "artifact-sha256", "publication", "integration-task",
                 "integration-owner", "integration-token-file", "direct-user-authorization"):
        retire.add_argument("--" + name, type=nonempty)
    unretire = commands.add_parser(
        "unretire", help="return a retired task to the backlog; the retirement stays in the audit trail")
    unretire.add_argument("id", type=nonempty)
    unretire.add_argument("--owner", required=True, type=nonempty)
    unretire.add_argument("--reason", required=True, type=nonempty)
    expire = commands.add_parser(
        "expire",
        help="end a live claim now instead of waiting for its lease to lapse; "
             "the task returns to pending and any old token stops working")
    expire.add_argument("id", type=nonempty)
    expire.add_argument("--owner", required=True, type=nonempty,
                        help="the current owner, which must match the claim")
    expire.add_argument("--reason", required=True, type=nonempty)
    ready = commands.add_parser(
        "ready",
        help="record a reviewer's ready_to_integrate verdict on the current submission; the "
             "task stays submitted with its locks until the publication step's `finish`")
    ready.add_argument("id", type=nonempty)
    ready.add_argument("--submission-id", required=True, type=nonempty)
    ready.add_argument("--reviewer", required=True, type=nonempty)
    ready.add_argument("--report", required=True, type=nonempty,
                       help="the review report file; its absolute path and SHA-256 are recorded")
    ready.add_argument("--note", required=True, type=nonempty)
    unready = commands.add_parser(
        "unready", help="withdraw a ready_to_integrate mark; the task stays submitted")
    unready.add_argument("id", type=nonempty)
    unready.add_argument("--submission-id", required=True, type=nonempty)
    unready.add_argument("--reviewer", required=True, type=nonempty)
    unready.add_argument("--reason", required=True, type=nonempty)
    finish = commands.add_parser("finish")
    finish.add_argument("id", type=nonempty)
    finish.add_argument("--submission-id", required=True, type=nonempty)
    finish.add_argument("--reviewer", type=nonempty)
    finish.add_argument("--decision", required=True, choices=("accept", "rework"))
    finish.add_argument("--note", required=True, type=nonempty)
    finish.add_argument("--publication", type=nonempty,
                        help="publication evidence (e.g. the integrator's publication.json) "
                             "naming this task; required to accept a TU candidate")
    finish.add_argument("--accept-without-publication", type=nonempty,
                        dest="accept_without_publication", metavar="REASON",
                        help="explicit, audited override of the publication requirement")
    finish.add_argument("--closed-by-integration", type=nonempty, dest="closed_by_integration",
                        metavar="INTEGRATION_TASK",
                        help="the publication step records the reviewer's accept (user direction "
                             "2026-09-18): requires --decision accept, an open ready mark by "
                             "--reviewer on this submission, a live tu-publication/1 --publication "
                             "that names the task, the submission and the mark's report SHA-256, "
                             "and the live claim of INTEGRATION_TASK holding baseline:integration "
                             "(--integration-owner, --integration-token-file)")
    finish.add_argument("--integration-owner", type=nonempty, dest="integration_owner")
    finish.add_argument("--integration-token-file", type=nonempty, dest="integration_token_file",
                        help="private 0600 file holding the integration claim's lease token")
    finish.add_argument("--closed-by-direct-acceptance", type=nonempty,
                        dest="closed_by_direct_acceptance", metavar="INTEGRATION_TASK",
                        help="close only a directly published candidate under an explicit user review waiver")
    finish.add_argument("--direct-user-authorization", type=nonempty,
                        dest="direct_user_authorization",
                        help="exact user directive recorded in the live direct-acceptance publication")
    finish.add_argument("--complete-publication-task", action="store_true",
                        help="administratively close the publisher's own submitted live publication; "
                             "does not review or accept any source candidate")
    return result


def connect(path, create):
    path = Path(path)
    if not path.is_absolute():
        raise QueueError("--db must be an absolute path shared by all sessions")
    if create:
        path.parent.mkdir(parents=True, exist_ok=True)
    # mode=rw is essential: a typo must not create an empty database.
    mode = "rwc" if create else "rw"
    # A generous busy timeout: sqlite's own busy handler waits for the lock
    # instead of failing after 10 s while another session (a module_dispatch
    # wave, a batch of closures) holds a long write transaction. Lock refusals
    # still surface as "database is locked" after BUSY_TIMEOUT_SECONDS and are
    # retried by execute_with_retry / --busy-retry.
    db = sqlite3.connect(f"{path.as_uri()}?mode={mode}", uri=True,
                         timeout=BUSY_TIMEOUT_SECONDS, isolation_level=None)
    db.execute(f"PRAGMA busy_timeout = {int(BUSY_TIMEOUT_SECONDS * 1000)}")
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys = ON")
    ensure_indexes(db)
    return db


def ensure_indexes(db, any_size=False):
    """Create any missing read-accelerator index (see INDEXES); never fails the caller.

    The common case is one read of sqlite_master. Each missing index is built
    in its own short BEGIN IMMEDIATE that waits at most INDEX_BUSY_MS for the
    write lock; a busy queue or a database without the tables (before `init`)
    is left alone. Building an index holds the write lock while it reads the
    whole table, so an ordinary connection only builds them on a queue smaller
    than AUTO_INDEX_MAX_BYTES (tests, scratch queues); a large production queue
    gets them from `init` (any_size), run once by an operator.
    """
    try:
        rows = db.execute("SELECT type, name FROM sqlite_master "
                          "WHERE type IN ('table', 'index')").fetchall()
        tables = {row[1] for row in rows if row[0] == "table"}
        present = {row[1] for row in rows if row[0] == "index"}
        if (not INDEX_TABLES <= tables
                or db.execute("PRAGMA user_version").fetchone()[0] not in (1, VERSION)):
            return []
        missing = [(name, statement) for name, statement in INDEXES if name not in present]
        if not missing:
            return []
        if not any_size:
            pages = db.execute("PRAGMA page_count").fetchone()[0]
            size = db.execute("PRAGMA page_size").fetchone()[0]
            if pages * size > AUTO_INDEX_MAX_BYTES:
                return []
    except sqlite3.Error:
        return []
    created = []
    cache_size = db.execute("PRAGMA cache_size").fetchone()[0]
    temp_store = db.execute("PRAGMA temp_store").fetchone()[0]
    try:
        db.execute(f"PRAGMA busy_timeout = {INDEX_BUSY_MS}")
        # Read the indexed column once outside any transaction (WAL readers
        # never block writers) into a large page cache and sort in memory, so
        # the write lock is held for the in-memory build only: on the 1.1M-row
        # resources table this is ~2 s instead of ~120 s under host IO load.
        db.execute(f"PRAGMA cache_size = {INDEX_CACHE_KIB}")
        db.execute("PRAGMA temp_store = MEMORY")
        for name, statement in missing:
            target = re.search(r" ON (\w+)\((\w+)", statement)
            if target:
                try:
                    db.execute(f"SELECT count(*) FROM {target.group(1)} "
                               f"WHERE {target.group(2)} IS NOT NULL").fetchone()
                except sqlite3.Error:
                    pass
            try:
                db.execute("BEGIN IMMEDIATE")
            except sqlite3.Error:
                break
            try:
                db.execute(statement)
                db.execute("COMMIT")
                created.append(name)
            except sqlite3.Error:
                db.execute("ROLLBACK")
                break
    finally:
        db.execute(f"PRAGMA cache_size = {cache_size}")
        db.execute(f"PRAGMA temp_store = {temp_store}")
        db.execute(f"PRAGMA busy_timeout = {int(BUSY_TIMEOUT_SECONDS * 1000)}")
    return created


def audit(db, now, action, task_id=None, actor=None, **detail):
    db.execute("INSERT INTO audit(at, action, task_id, actor, detail) VALUES (?, ?, ?, ?, ?)",
               (now, action, task_id, actor, json.dumps(detail, sort_keys=True)))


def task_row(db, task_id):
    row = db.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
    if row is None:
        raise QueueError(f"unknown task: {task_id}")
    return row


def has_table(db, name):
    return bool(db.execute(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?", (name,)).fetchone())


def has_retired_table(db):
    return has_table(db, "retired")


def retired_row(db, task_id):
    """The retirement record of one task, or None. Absent table means none."""
    if not has_retired_table(db):
        return None
    row = db.execute("SELECT * FROM retired WHERE task_id = ?", (task_id,)).fetchone()
    return dict(row) if row else None


def ensure_ready_table(db, now, actor=None):
    """Create the additive integration_ready table once, inside the caller's write transaction."""
    if has_table(db, "integration_ready"):
        return False
    for statement in READY_SCHEMA:
        db.execute(statement)
    audit(db, now, "migrate", None, actor, added=["integration_ready"], user_version=VERSION)
    return True


def open_ready_row(db, task_id, submission_id):
    """The open ready mark of one submission, or None. Absent table means none."""
    if not has_table(db, "integration_ready"):
        return None
    row = db.execute("SELECT * FROM integration_ready WHERE task_id = ? AND submission_id = ? "
                     "AND closed_at IS NULL", (task_id, submission_id)).fetchone()
    return dict(row) if row else None


def task_json(db, task_id, include_token=False):
    task = dict(task_row(db, task_id))
    # The stored token is a worker lease while claimed, a public review ID after submit.
    task["submission_id"] = task["token"] if task["status"] in ("submitted", "done") else None
    if not include_token:
        del task["token"]
    task["resources"] = [row[0] for row in db.execute(
        "SELECT resource FROM resources WHERE task_id = ? ORDER BY resource", (task_id,))]
    task["depends_on"] = [row[0] for row in db.execute(
        "SELECT depends_on FROM dependencies WHERE task_id = ? ORDER BY depends_on", (task_id,))]
    # Never folded into `status`: a retirement is not an acceptance.
    task["retired"] = retired_row(db, task_id)
    # Only a mark on the *current* submission of a still-submitted task counts.
    task["ready_to_integrate"] = (open_ready_row(db, task_id, task["submission_id"])
                                  if task["status"] == "submitted" else None)
    if has_table(db, 'submitted_revisions'):
        revision = submitted_revision(db, task_id)
        if revision:
            task['submitted_revision'] = dict(revision)
        successors = db.execute(
            'SELECT successor_task,resource FROM submitted_revisions WHERE predecessor_task=?',
            (task_id,)).fetchall()
        if successors:
            task['submitted_successors'] = [dict(row) for row in successors]
    return task


def make_pending(db, task_id, now):
    revision = validate_revision_claim(db, task_id)
    if revision:
        # Restore the immutable submission's reservation inside this same transaction.
        changed = db.execute('UPDATE locks SET task_id=? WHERE resource=? AND task_id=?',
                             (revision['predecessor_task'], revision['resource'], task_id)).rowcount
        if changed != 1:
            raise QueueError('submitted revision lost its delegated function lock')
        audit(db, now, 'submitted_revision_restore', task_id, task_row(db, task_id)['owner'],
              predecessor=revision['predecessor_task'], resource=revision['resource'],
              predecessor_submission_id=revision['predecessor_submission_id'])
    db.execute("DELETE FROM locks WHERE task_id = ?", (task_id,))
    db.execute("""UPDATE tasks SET status = 'pending', owner = NULL, token = NULL,
                  lease_until = NULL, artifact = NULL, updated_at = ? WHERE id = ?""",
               (now, task_id))


# --------------------------------------------------------------------------
# Token files
# --------------------------------------------------------------------------

def private_token_path(value):
    """Resolve a token-file path and refuse locations other agents can reach.

    The directory must exist, be owned by this user and not be group/other
    writable; shared temporary roots (/tmp, /var/tmp, /dev/shm, $TMPDIR) and
    session scratchpads are refused because agents of one session share them.
    An existing file must be a regular, single-link file owned by this user.
    """
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    try:
        parent = path.parent.resolve(strict=True)
    except (FileNotFoundError, NotADirectoryError):
        raise QueueError(f"token file directory does not exist: {path.parent}") from None
    if not parent.is_dir():
        raise QueueError(f"token file directory is not a directory: {parent}")
    real = parent / path.name
    roots = list(SHARED_TEMP_ROOTS)
    if os.environ.get("TMPDIR"):
        roots.append(os.environ["TMPDIR"])
    for root in roots:
        try:
            shared = Path(root).resolve()
        except OSError:
            continue
        if real == shared or shared in real.parents:
            raise QueueError(f"refusing a token file under the shared temporary root {shared}; "
                             "use a private directory such as .work/reviews/<task>/")
    if any(part.lower() in SHARED_PATH_PARTS for part in real.parts):
        raise QueueError("refusing a token file inside a session scratchpad (shared by every "
                         "agent of the session); use a private directory such as "
                         ".work/reviews/<task>/")
    info = os.stat(parent)
    if info.st_uid != os.getuid():
        raise QueueError(f"token file directory is not owned by this user: {parent}")
    if info.st_mode & 0o022:
        raise QueueError(f"token file directory is group/other writable: {parent}")
    if os.path.lexists(real):
        existing = os.lstat(real)
        if stat.S_ISLNK(existing.st_mode):
            raise QueueError(f"token file is a symlink: {real}")
        if not stat.S_ISREG(existing.st_mode):
            raise QueueError(f"token file is not a regular file: {real}")
        if existing.st_nlink != 1:
            raise QueueError(f"token file is hardlinked elsewhere: {real}")
        if existing.st_uid != os.getuid():
            raise QueueError(f"token file is not owned by this user: {real}")
    return real


def read_token_file(value):
    path = private_token_path(value)
    if not path.exists():
        raise QueueError(f"token file does not exist: {path}")
    if os.stat(path).st_mode & 0o077:
        raise QueueError(f"token file is readable by others (need mode 0600): {path}")
    token = path.read_text(encoding="ascii").strip()
    if len(token) != 64 or any(c not in "0123456789abcdef" for c in token):
        raise QueueError(f"token file does not hold a lease token: {path}")
    return token


def existing_token(path):
    try:
        text = path.read_text(encoding="ascii").strip()
    except (OSError, UnicodeDecodeError):
        return None
    return text or None


def refuse_live_token_clobber(db, path, task_id=None):
    """Never overwrite a file holding the token of another live claim."""
    token = existing_token(path) if path.exists() else None
    if token is None:
        return
    row = db.execute("SELECT id FROM tasks WHERE status = 'claimed' AND token = ?",
                     (token,)).fetchone()
    if row is not None and row["id"] != task_id:
        raise QueueError(f"token file {path} holds the live token of task {row['id']}; "
                         "use a separate private file per claim")


def write_token_file(path, token):
    """Atomic 0600 write: exclusive temp file in the same directory, fsync, rename."""
    temp = path.parent / f".{path.name}.{os.getpid()}.{secrets.token_hex(4)}.tmp"
    handle = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(handle, "w", encoding="ascii") as stream:
            stream.write(token + "\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    except BaseException:
        try:
            os.unlink(temp)
        except OSError:
            pass
        raise
    directory = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def file_evidence(value, what):
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    path = path.resolve()
    if not path.is_file():
        raise QueueError(f"{what} is not a readable file: {path}")
    data = path.read_bytes()
    return path, data, hashlib.sha256(data).hexdigest()


def submitted_revision(db, task_id):
    if not has_table(db, 'submitted_revisions'):
        return None
    return db.execute('SELECT * FROM submitted_revisions WHERE successor_task=?',
                      (task_id,)).fetchone()


def protect_revision_predecessor(db, task_id):
    if (has_table(db, 'submitted_revisions') and db.execute(
            'SELECT 1 FROM submitted_revisions WHERE predecessor_task=? LIMIT 1',
            (task_id,)).fetchone()):
        raise QueueError('submission has an immutable revision successor; operate on its current successor')


def revision_identity(data, revision, what):
    target = data.get('target') or {}
    exact = [f for f in data.get('functions', []) if f.get('state') == 'exact_candidate']
    if (target.get('unit') != revision['unit'] or
            target.get('binary_sha256') != revision['binary_sha256'] or len(exact) != 1):
        raise QueueError(what + ' must contain exactly one exact candidate for the pinned IOP binary')
    function = exact[0]
    if (function.get('address') != revision['address'] or
            revision['function_name'] not in (function.get('original_names') or []) or
            not function.get('candidate_artifacts')):
        raise QueueError(what + ' function identity or immutable candidate artifacts differ')


def validate_revision_parent(db, revision):
    parent = task_row(db, revision['predecessor_task'])
    if (parent['status'] != 'submitted' or parent['token'] != revision['predecessor_submission_id']
            or parent['owner'] != revision['predecessor_owner']):
        raise QueueError('revision predecessor submission changed')
    path, data, digest = file_evidence(parent['artifact'] or '', 'revision predecessor artifact')
    if (str(path) != revision['predecessor_artifact'] or
            digest != revision['predecessor_artifact_sha256']):
        raise QueueError('revision predecessor artifact changed')
    try:
        revision_identity(json.loads(data), revision, 'revision predecessor')
    except (ValueError, TypeError, AttributeError) as exc:
        raise QueueError('revision predecessor artifact is invalid: ' + str(exc)) from exc


def validate_revision_claim(db, task_id):
    revision = submitted_revision(db, task_id)
    if not revision:
        return None
    validate_revision_parent(db, revision)
    lock = db.execute('SELECT task_id FROM locks WHERE resource=?',
                      (revision['resource'],)).fetchone()
    if not lock or lock['task_id'] != task_id:
        raise QueueError('revision claim does not exclusively own its delegated function lock')
    return revision


def revise_submitted_claim(db, args, now):
    """Called inside BEGIN IMMEDIATE; preserve predecessor, delegate one IOP lock.

    Identities are caller-supplied, as for the existing owner operations. The
    expected submission ID, immutable artifact digest and original identity
    prevent stale or accidental delegation. This operation confers no verdict.
    """
    if not db.in_transaction:
        raise QueueError('revise-submitted requires a caller-owned write transaction')
    if (args.unit not in ('iop_ssd', 'iop_rssd') or
            not re.fullmatch(r'[0-9a-f]{64}', args.binary_sha256) or
            not re.fullmatch(r'[0-9a-f]{64}', args.artifact_sha256) or
            not re.fullmatch(r'0x[0-9a-f]{8}', args.address) or
            not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', args.function) or args.id == args.predecessor):
        raise QueueError('invalid singleton IOP submitted-revision identity')
    lease = lease_seconds(args.lease_seconds)
    parent = task_row(db, args.predecessor)
    if parent['status'] != 'submitted' or parent['owner'] != args.owner or parent['token'] != args.submission_id:
        raise QueueError('revision requires the pinned submitted predecessor and its owner')
    path, data, digest = file_evidence(parent['artifact'] or '', 'revision predecessor artifact')
    if digest != args.artifact_sha256:
        raise QueueError('revision predecessor artifact SHA-256 differs')
    resource = f'func:{args.binary_sha256}:{args.unit}:{args.address}'
    lineage = dict(successor_task=args.id, predecessor_task=args.predecessor,
                   predecessor_submission_id=args.submission_id, predecessor_artifact=str(path),
                   predecessor_artifact_sha256=digest, predecessor_owner=args.owner,
                   resource=resource, binary_sha256=args.binary_sha256, unit=args.unit,
                   address=args.address, function_name=args.function)
    try:
        revision_identity(json.loads(data), lineage, 'revision predecessor')
    except (ValueError, TypeError, AttributeError) as exc:
        raise QueueError('revision predecessor artifact is invalid: ' + str(exc)) from exc
    if not db.execute('SELECT 1 FROM resources WHERE task_id=? AND resource=?',
                      (args.predecessor, resource)).fetchone():
        raise QueueError('predecessor did not reserve the pinned original function')
    lock = db.execute('SELECT task_id FROM locks WHERE resource=?', (resource,)).fetchone()
    if not lock or lock['task_id'] != args.predecessor:
        raise QueueError('predecessor no longer exclusively owns the original function lock')
    extra = sorted(set(getattr(args, 'resource', None) or []))
    prefix = '.work/module-workers/' + args.id + '/attempt-'
    for item in extra:
        kind, separator, relative = item.partition(':')
        parts = Path(relative).parts
        if (kind not in ('file', 'sandbox') or not separator or not relative.startswith(prefix)
                or Path(relative).is_absolute() or any(p in ('.', '..') for p in parts)
                or any(c in relative for c in '*?[]\\') or Path(relative).as_posix() != relative):
            raise QueueError('revision private resources must be literal fresh successor attempt paths')
        if db.execute('SELECT 1 FROM resources WHERE resource=? LIMIT 1', (item,)).fetchone():
            raise QueueError('revision private resource is not fresh: ' + item)
    token_file = getattr(args, 'token_file', None)
    token_path = private_token_path(token_file) if token_file else None
    if token_path is not None:
        refuse_live_token_clobber(db, token_path)
    existed = has_table(db, 'submitted_revisions')
    db.execute(SUBMITTED_REVISION_SCHEMA)
    if not existed:
        audit(db, now, 'migrate-submitted-revisions', None, args.owner)
    previous = submitted_revision(db, args.id)
    child = db.execute('SELECT * FROM tasks WHERE id=?', (args.id,)).fetchone()
    if previous:
        if any(previous[key] != value for key, value in lineage.items()):
            raise QueueError('successor identity or predecessor pins changed')
        if child['status'] != 'pending' or retired_row(db, args.id):
            raise QueueError('only the same pending successor can be reclaimed')
    else:
        if child or db.execute('SELECT 1 FROM submitted_revisions WHERE predecessor_task=? '
                               'AND predecessor_submission_id=? AND resource=?',
                               (args.predecessor, args.submission_id, resource)).fetchone():
            raise QueueError('submitted revision successor already exists; forks are forbidden')
        db.execute("INSERT INTO tasks VALUES (?,?,0,'pending',NULL,NULL,NULL,NULL,?,?)",
                   (args.id, 'Matching revision of ' + args.predecessor, now, now))
        fields = dict(lineage, created_at=now, actor=args.owner, reason=args.reason)
        db.execute('INSERT INTO submitted_revisions (' + ','.join(fields) + ') VALUES (' +
                   ','.join('?' for _ in fields) + ')', tuple(fields.values()))
        db.execute('INSERT INTO resources(task_id,resource) VALUES (?,?)', (args.id, resource))
    token = secrets.token_hex(32)
    until = now + lease
    db.execute("UPDATE tasks SET status='claimed',owner=?,token=?,lease_until=?,artifact=NULL,updated_at=? WHERE id=?",
               (args.owner, token, until, now, args.id))
    changed = db.execute('UPDATE locks SET task_id=? WHERE resource=? AND task_id=?',
                         (args.id, resource, args.predecessor)).rowcount
    if changed != 1:
        raise QueueError('revision delegation lost the predecessor lock')
    for item in extra:
        db.execute('INSERT INTO resources(task_id,resource) VALUES (?,?)', (args.id, item))
        db.execute('INSERT INTO locks(resource,task_id) VALUES (?,?)', (item, args.id))
    audit(db, now, 'claim', args.id, args.owner, lease_until=until,
          submitted_revision=True, predecessor=args.predecessor)
    audit(db, now, 'revise-submitted', args.id, args.owner, **lineage, reason=args.reason,
          private_resources=extra, reclaimed=bool(previous), review_performed=False,
          source_acceptance_performed=False)
    if token_path is not None:
        write_token_file(token_path, token)
    result = {'task': task_json(db, args.id, include_token=token_path is None), 'revision': lineage}
    if token_path is not None:
        result['token_file'] = str(token_path)
    return result


# --------------------------------------------------------------------------
# Commands
# --------------------------------------------------------------------------

def add_task(db, args, now):
    if db.execute("SELECT 1 FROM tasks WHERE id = ?", (args.id,)).fetchone():
        raise QueueError(f"duplicate task: {args.id}")
    resources = sorted(set(args.resource))
    dependencies = sorted(set(args.depends_on))
    # Only existing tasks can be dependencies; no edits means no cycles.
    for dependency in dependencies:
        task_row(db, dependency)
    db.execute("""INSERT INTO tasks(id, title, priority, status, created_at, updated_at)
                  VALUES (?, ?, ?, 'pending', ?, ?)""",
               (args.id, args.title, args.priority, now, now))
    db.executemany("INSERT INTO resources(task_id, resource) VALUES (?, ?)",
                   [(args.id, resource) for resource in resources])
    db.executemany("INSERT INTO dependencies(task_id, depends_on) VALUES (?, ?)",
                   [(args.id, dependency) for dependency in dependencies])
    audit(db, now, "add", args.id, title=args.title, priority=args.priority,
          resources=resources, depends_on=dependencies)
    return {"task": task_json(db, args.id)}


def claim_task(db, args, now):
    # `token_file` is optional so in-process callers (module_dispatch) keep working.
    token_file = getattr(args, "token_file", None)
    token_path = private_token_path(token_file) if token_file else None
    if args.task is not None:
        task_row(db, args.task)
        if submitted_revision(db, args.task):
            raise QueueError('submitted successors require revise-submitted, not generic claim')
    if token_path is not None:
        refuse_live_token_clobber(db, token_path)
    expired = db.execute("SELECT id, owner, lease_until FROM tasks "
                         "WHERE status = 'claimed' AND lease_until <= ?", (now,)).fetchall()
    for row in expired:
        make_pending(db, row["id"], now)
        audit(db, now, "expire", row["id"], args.owner,
              previous_owner=row["owner"], lease_until=row["lease_until"])
    # A retired task is superseded work, never claimable again until `unretire`.
    # Spelled conditionally so a queue that predates the table still claims.
    skip_retired = ("AND NOT EXISTS (SELECT 1 FROM retired x WHERE x.task_id = t.id)"
                    if has_retired_table(db) else "")
    skip_revisions = ("AND NOT EXISTS (SELECT 1 FROM submitted_revisions v WHERE v.successor_task=t.id)"
                      if has_table(db, 'submitted_revisions') else "")
    # A named task is looked up by its primary key; `(? IS NULL OR t.id = ?)`
    # selected the same row but made sqlite scan every task to find it.
    which = "t.id = ?" if args.task is not None else "1"
    row = db.execute(f"""
        SELECT t.id FROM tasks t
        WHERE t.status = 'pending' AND {which}
          {skip_retired}
          {skip_revisions}
          AND NOT EXISTS (
            SELECT 1 FROM dependencies d JOIN tasks parent ON parent.id = d.depends_on
            WHERE d.task_id = t.id AND parent.status != 'done'
          )
          AND NOT EXISTS (
            SELECT 1 FROM resources r JOIN locks l ON l.resource = r.resource
            WHERE r.task_id = t.id
          )
        ORDER BY t.priority DESC, t.created_at ASC, t.id ASC LIMIT 1
    """, (args.task,) if args.task is not None else ()).fetchone()
    if row is None:
        return {"task": None}
    task_id = row["id"]
    token = secrets.token_hex(32)
    until = now + args.lease_seconds
    db.execute("""UPDATE tasks SET status = 'claimed', owner = ?, token = ?,
                  lease_until = ?, updated_at = ? WHERE id = ?""",
               (args.owner, token, until, now, task_id))
    db.execute("INSERT INTO locks(resource, task_id) "
               "SELECT resource, task_id FROM resources WHERE task_id = ?", (task_id,))
    audit(db, now, "claim", task_id, args.owner, lease_until=until,
          **({"token_file": str(token_path)} if token_path else {}))
    if token_path is None:
        return {"task": task_json(db, task_id, include_token=True)}
    # Written inside the transaction: a failed write rolls the claim back, so
    # no claim is ever committed with its token lost.
    write_token_file(token_path, token)
    return {"task": task_json(db, task_id), "token_file": str(token_path)}


def reclaim_claim(db, args, now):
    """Rotate the token of a claim whose owner lost it, keeping the locks.

    `expire` followed by `claim` also recovers a lost token, but between the
    two any other claimant can take the task. `reclaim` does it in one
    transaction: the same owner, a claim not yet swept back to `pending`
    (a lapsed lease counts), a fresh token written to a private token file and
    a fresh lease. The old token stops working, and the reason is audited.
    """
    if submitted_revision(db, args.id):
        raise QueueError('expire and revise-submitted to recover a successor token without unlocking its predecessor')
    token_path = private_token_path(args.token_file)
    row = task_row(db, args.id)
    if row["status"] == "pending":
        raise QueueError(f"{args.id} is pending (its claim was already swept); claim it again "
                         "with `claim --task ID --owner OWNER --token-file PATH`")
    if row["status"] != "claimed":
        raise QueueError(f"reclaim requires a claimed task; {args.id} is {row['status']}")
    if row["owner"] != args.owner:
        raise QueueError("reclaim requires the current owner of the claim")
    refuse_live_token_clobber(db, token_path, task_id=args.id)
    token = secrets.token_hex(32)
    until = now + args.lease_seconds
    db.execute("UPDATE tasks SET token = ?, lease_until = ?, updated_at = ? WHERE id = ?",
               (token, until, now, args.id))
    # A claimed task keeps its locks; restore any that are missing (none
    # should be) without ever taking a lock another task holds.
    db.execute("INSERT OR IGNORE INTO locks(resource, task_id) "
               "SELECT resource, task_id FROM resources WHERE task_id = ?", (args.id,))
    foreign = [r[0] for r in db.execute(
        "SELECT r.resource FROM resources r JOIN locks l ON l.resource = r.resource "
        "WHERE r.task_id = ? AND l.task_id != ?", (args.id, args.id))]
    if foreign:
        raise QueueError("resources of this claim are locked by another task: " + ", ".join(foreign))
    audit(db, now, "reclaim", args.id, args.owner, reason=args.reason, lease_until=until,
          previous_lease_until=row["lease_until"], lapsed=row["lease_until"] <= now,
          token_file=str(token_path))
    write_token_file(token_path, token)
    return {"task": task_json(db, args.id), "token_file": str(token_path)}


def narrow_claim(db, args, now):
    """Release part of a live allocation without ending the claim.

    A worker that recovered some of its allocated functions and reached a
    ceiling on the rest submits the exact subset and hands the remainder back.
    Without this the whole allocation stays locked behind one submission, so the
    unresolved functions cannot be re-planned until the review closes. Dropping
    a resource releases its lock immediately; everything the task still holds
    keeps working exactly as before, so the claim, the lease and the token are
    untouched. The dropped list is recorded in the audit trail, and the
    dispatcher reads it: a task that legitimately holds fewer resources than its
    plan job lists is a narrowed allocation, not a corrupted one.
    """
    if submitted_revision(db, args.id):
        raise QueueError('a singleton submitted successor cannot narrow its delegated reservation')
    row = task_row(db, args.id)
    if (row["status"] != "claimed" or row["owner"] != args.owner
            or row["token"] != args.token or row["lease_until"] <= now):
        raise QueueError("requires a claimed task with matching owner, token and unexpired lease")
    held = {r[0] for r in db.execute(
        "SELECT resource FROM resources WHERE task_id = ?", (args.id,))}
    drop = sorted(set(args.drop_resource))
    unknown = [resource for resource in drop if resource not in held]
    if unknown:
        raise QueueError("not a resource of this task: " + ", ".join(unknown))
    if not held - set(drop):
        raise QueueError("narrow must leave at least one resource; release the task instead")
    for resource in drop:
        db.execute("DELETE FROM locks WHERE resource = ? AND task_id = ?", (resource, args.id))
        db.execute("DELETE FROM resources WHERE task_id = ? AND resource = ?", (args.id, resource))
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "narrow", args.id, args.owner, dropped=drop, reason=args.reason,
          remaining=sorted(held - set(drop)))
    return {"task": task_json(db, args.id), "dropped": drop}


def narrow_submitted(db, args, now):
    """Repair only undelivered function locks, without reviewing the candidate.

    A worker should narrow *before* submission. If it failed to, a submitted
    whole-TU allocation can retain unrelated func: locks indefinitely. This
    guarded repair reads the pinned, immutable result and will not release any
    function that result calls an exact candidate. It never changes the result,
    source, TU reservation, submission ID, review status or candidate locks.
    """
    protect_revision_predecessor(db, args.id)
    if submitted_revision(db, args.id):
        raise QueueError('a submitted successor retains its complete singleton reservation')
    row = task_row(db, args.id)
    if (row["status"] != "submitted" or row["owner"] != args.owner
            or row["token"] != args.submission_id):
        raise QueueError("requires the current submitted task, owner and submission ID")
    path = Path(row["artifact"] or "")
    if not path.is_file():
        raise QueueError("submitted artifact is not a file")
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != args.artifact_sha256.lower():
        raise QueueError("submitted artifact SHA-256 differs; preserve it and investigate")
    try:
        result = json.loads(data)
    except (ValueError, UnicodeDecodeError) as exc:
        raise QueueError(f"submitted artifact is not valid JSON: {exc}") from exc
    entries = result.get("functions")
    if not isinstance(entries, list) or not entries:
        raise QueueError("submitted artifact has no function-level result entries")
    identities = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("origin_tu"), str):
            raise QueueError("submitted artifact has an incomplete function identity")
        unit = entry["origin_tu"].split("/")[0]
        va = entry.get("address")
        if not isinstance(va, str) or not va.startswith("0x"):
            raise QueueError("submitted artifact has an incomplete function address")
        identity = f"{unit}:{va.lower()}"
        if identity in identities:
            raise QueueError("submitted artifact has a duplicate function identity")
        identities[identity] = entry.get("state")
    has_candidate = "exact_candidate" in identities.values()
    if args.analysis_only:
        if has_candidate or result.get("evidence_level") != "analysis_only":
            raise QueueError("--analysis-only requires a submitted analysis_only artifact "
                             "with no exact candidates")
    elif not has_candidate:
        raise QueueError("no exact candidates to retain: reconcile the submission first "
                         "(or use --analysis-only to drop every undelivered func: lock)")
    held = {r[0] for r in db.execute(
        "SELECT resource FROM resources WHERE task_id = ?", (args.id,))}
    drop = sorted(set(args.drop_resource or []))
    if not drop and not args.rebase_slice_to:
        raise QueueError("name --drop-resource or --rebase-slice-to; nothing to repair")
    if args.analysis_only and set(drop) != {r for r in held if r.startswith("func:")}:
        raise QueueError("--analysis-only must drop every held func: lock; "
                         "the immutable submission delivers no candidates")
    rebase = None
    if args.rebase_slice_to:
        if args.analysis_only or result.get("evidence_level") != "exact_candidate":
            raise QueueError("slice rebasing requires an exact-candidate submission")
        tus = {entry["origin_tu"] for entry in entries}
        if len(tus) != 1:
            raise QueueError("slice rebasing requires one original TU")
        tu = next(iter(tus))
        markers = [r for r in held if r.startswith(f"otuslice:{tu}/")]
        if len(markers) != 1:
            raise QueueError("slice rebasing requires exactly one held otuslice: marker")
        old = markers[0]
        old_va = old.rsplit("/", 1)[1].lower()
        new_va = args.rebase_slice_to.lower()
        try:
            if len(new_va) != 10 or not new_va.startswith("0x"):
                raise ValueError("VA must have eight hexadecimal digits")
            int(new_va[2:], 16)
        except ValueError as exc:
            raise QueueError(f"invalid --rebase-slice-to {args.rebase_slice_to}: {exc}") from exc
        unit = tu.split("/", 1)[0]
        candidate_vas = sorted(int(identity.split(":", 1)[1], 16)
                               for identity, state in identities.items()
                               if identity.startswith(unit + ":") and state == "exact_candidate")
        if (identities.get(f"{unit}:{old_va}") == "exact_candidate"
                or f"{unit}:{old_va}" not in identities
                or any(r.startswith("func:") and r.endswith(f":{unit}:{old_va}") for r in held)
                or not candidate_vas or int(new_va, 16) != candidate_vas[0]
                or not any(r.startswith("func:") and r.endswith(f":{unit}:{new_va}")
                           for r in held)):
            raise QueueError("slice marker can move only from a documented, unlocked "
                             "undelivered first function to the first locked exact candidate")
        new = f"otuslice:{tu}/{new_va}"
        if new == old or db.execute("SELECT 1 FROM locks WHERE resource = ?", (new,)).fetchone():
            raise QueueError("the replacement slice marker is unchanged or already reserved")
        rebase = (old, new)
    if any(not resource.startswith("func:") for resource in drop):
        raise QueueError("only undelivered func: locks can be repaired")
    for resource in drop:
        if resource not in held:
            raise QueueError(f"not a held resource: {resource}")
        parts = resource.split(":")
        identity = ":".join(parts[-2:]).lower() if len(parts) == 4 else ""
        if identity not in identities or identities[identity] == "exact_candidate":
            raise QueueError(f"not a documented undelivered function: {resource}")
    retained = held - set(drop)
    for identity, state in identities.items():
        if state == "exact_candidate" and not any(
                resource.startswith("func:") and resource.endswith(":" + identity)
                for resource in retained):
            raise QueueError(f"submitted candidate would lose its function lock: {identity}")
    for resource in drop:
        db.execute("DELETE FROM locks WHERE resource = ? AND task_id = ?", (resource, args.id))
        db.execute("DELETE FROM resources WHERE task_id = ? AND resource = ?",
                   (args.id, resource))
    if rebase:
        old, new = rebase
        db.execute("UPDATE locks SET resource = ? WHERE resource = ? AND task_id = ?",
                   (new, old, args.id))
        db.execute("UPDATE resources SET resource = ? WHERE resource = ? AND task_id = ?",
                   (new, old, args.id))
        retained = (retained - {old}) | {new}
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "narrow_submitted", args.id, args.operator or args.owner, dropped=drop,
          reason=args.reason, submission_id=args.submission_id,
          submitted_owner=args.owner,
          artifact=row["artifact"], artifact_sha256=digest,
          analysis_only=args.analysis_only,
          slice_rebase=dict(old=rebase[0], new=rebase[1]) if rebase else None,
          remaining=sorted(retained))
    return {"task": task_json(db, args.id), "dropped": drop}


def retire_submitted_superseded(db, row, successor, actor, reason, now, evidence):
    """Retire obsolete submitted history without accepting it or erasing its pins."""
    task_id = row["id"]
    if row["status"] != "submitted" or retired_row(db, task_id):
        raise QueueError("supersession requires a current unretired submission")
    blockers = [r[0] for r in db.execute(
        "SELECT d.task_id FROM dependencies d JOIN tasks t ON t.id=d.task_id "
        "WHERE d.depends_on=? AND t.status!='done' AND d.task_id!=? "
        "AND d.task_id NOT IN (SELECT task_id FROM retired) ORDER BY d.task_id",
        (task_id, successor))]
    if blockers:
        raise QueueError("live tasks depend on the superseded submission: " + ", ".join(blockers))
    db.execute("DELETE FROM locks WHERE task_id=?", (task_id,))
    db.execute("UPDATE tasks SET status='pending', lease_until=NULL, updated_at=? WHERE id=?",
               (now, task_id))
    db.execute("INSERT INTO retired(task_id,at,actor,reason,superseded_by) VALUES (?,?,?,?,?)",
               (task_id, now, actor, reason, successor))
    mark = close_ready(db, task_id, row["token"], now, actor,
                       "superseded-by-published-submission", reason)
    audit(db, now, "retire", task_id, actor, reason=reason, superseded_by=successor,
          previous_status="submitted", submission_id=row["token"], artifact=row["artifact"],
          source_acceptance_performed=False, review_performed=False, closed_ready_mark=mark,
          **evidence)


def retire_published_supersession(db, args, row, now):
    required = ("submission_id", "artifact_sha256", "publication", "superseded_by",
                "integration_task", "integration_owner", "integration_token", "direct_user_authorization")
    if any(not getattr(args, name, None) for name in required):
        raise QueueError("published supersession requires submission/artifact pins, replacement, publication and live integration authority")
    if row["status"] != "submitted" or row["token"] != args.submission_id:
        raise QueueError("published supersession must pin the current submitted task")
    _, data, old_digest = file_evidence(row["artifact"] or "", "superseded artifact")
    old = json.loads(data)
    functions = old.get("functions") or []
    target = old.get("target") or {}
    if (old_digest != args.artifact_sha256 or target.get("unit") not in ("iop_ssd", "iop_rssd") or
            not re.fullmatch(r"[0-9a-f]{64}", target.get("binary_sha256") or "") or
            len(functions) != 1 or functions[0].get("state") != "experimenting" or
            functions[0].get("candidate_artifacts") or not functions[0].get("original_names") or
            not re.fullmatch(r"0x[0-9a-f]{8}", functions[0].get("address") or "")):
        raise QueueError("published supersession only permits a pinned zero-exact IOP singleton")
    replacement = task_row(db, args.superseded_by)
    if replacement["status"] != "done" or replacement["id"] == args.id:
        raise QueueError("superseding task must already be accepted and distinct")
    replacement_path, replacement_data, replacement_digest = file_evidence(
        replacement["artifact"] or "", "published replacement artifact")
    new = json.loads(replacement_data)
    if (new.get("target", {}).get("unit") != target.get("unit") or
            new.get("target", {}).get("binary_sha256") != target.get("binary_sha256") or
            not any(f.get("state") == "exact_candidate" and
                    f.get("address") == functions[0].get("address") and
                    set(f.get("original_names") or []).intersection(functions[0].get("original_names") or [])
                    for f in new.get("functions") or [])):
        raise QueueError("published replacement does not recover the same original IOP identity")
    integ = task_row(db, args.integration_task)
    lock = db.execute("SELECT task_id FROM locks WHERE resource=?", (INTEGRATION_RESOURCE,)).fetchone()
    if (args.owner != args.integration_owner or integ["status"] != "claimed" or
            integ["owner"] != args.integration_owner or integ["token"] != args.integration_token or
            integ["lease_until"] <= now or not lock or lock["task_id"] != args.integration_task):
        raise QueueError("published supersession requires its owner's live token-bound baseline integration claim")
    path, data, digest = file_evidence(args.publication, "--publication")
    pub = json.loads(data)
    if (pub.get("schema") != "tu-publication/1" or pub.get("dry_run") or pub.get("result") != "published" or
            (pub.get("locks") or {}).get("integration_task") != args.integration_task):
        raise QueueError("supersession publication is not the successful live integration")
    reports = {r.get("path") for r in pub.get("reports") or [] if isinstance(r, dict)}
    naming = [p for p in pub.get("published") or [] if isinstance(p, dict) and any(
        s.get("task") == args.superseded_by and s.get("submission_id") == replacement["token"]
        for s in p.get("sources") or [])]
    if not naming:
        raise QueueError("publication does not name the current replacement submission")
    for published in naming:
        basis = published.get("acceptance_basis") or {}
        if (basis.get("kind") != "user_authorized_review_waiver" or
                basis.get("directive") != args.direct_user_authorization or
                published.get("review") is not None or published.get("report") not in reports):
            raise QueueError("publication lacks the exact user-authorized replacement evidence")
        cited = [s for s in published.get("sources") or []
                 if s.get("task") == args.superseded_by and s.get("submission_id") == replacement["token"]]
        if any(Path(s.get("result", {}).get("path", "")).resolve() != replacement_path or
               s.get("result", {}).get("sha256") != replacement_digest for s in cited):
            raise QueueError("publication replacement artifact pin differs")
    retire_submitted_superseded(db, row, args.superseded_by, args.owner, args.reason, now,
        dict(artifact_sha256=old_digest, publication=str(path), publication_sha256=digest,
             replacement_submission_id=replacement["token"], replacement_artifact=str(replacement_path),
             replacement_artifact_sha256=replacement_digest,
             user_direct_authorization=args.direct_user_authorization, integration_task=args.integration_task))
    return {"task": task_json(db, args.id)}


def retire_task(db, args, now):
    """Record that a pending assignment is superseded and will never be done.

    The queue had two ways to leave the backlog: `done`, which means a reviewer
    accepted the work, and a claim lapsing back to `pending`. Neither fits a task
    whose work landed under another ID, that never had content, or whose subject
    no longer exists. Calling such a task `done` would report an acceptance that
    never happened, so retirement is recorded beside the task instead of inside
    its status, with the actor and the concrete reason in the audit trail.

    Only a `pending` task can be retired, so this can never take work away from a
    live claim, discard a submitted artifact or rewrite an acceptance. It holds
    no locks, and `claim` skips it until someone `unretire`s it.
    """
    if submitted_revision(db, args.id):
        raise QueueError('submitted revision successors cannot be retired')
    protect_revision_predecessor(db, args.id)
    row = task_row(db, args.id)
    if getattr(args, "published_supersession", False):
        return retire_published_supersession(db, args, row, now)
    if any(getattr(args, name, None) for name in
           ("submission_id", "artifact_sha256", "publication", "integration_task", "integration_owner",
            "integration_token_file", "direct_user_authorization")):
        raise QueueError("publication supersession arguments require --published-supersession")
    if row["status"] != "pending":
        raise QueueError(f"only a pending task can be retired; {args.id} is {row['status']}. "
                         "A claimed task is expired or released, a submitted one reviewed, "
                         "and an accepted one stays done.")
    if retired_row(db, args.id):
        raise QueueError(f"already retired: {args.id}")
    if args.superseded_by is not None:
        task_row(db, args.superseded_by)
        if args.superseded_by == args.id:
            raise QueueError("a task cannot supersede itself")
    blockers = [r[0] for r in db.execute(
        "SELECT d.task_id FROM dependencies d JOIN tasks t ON t.id = d.task_id "
        "WHERE d.depends_on = ? AND t.status != 'done' "
        "AND d.task_id NOT IN (SELECT task_id FROM retired) ORDER BY d.task_id", (args.id,))]
    if blockers:
        raise QueueError("live tasks depend on this one and would never become claimable: "
                         + ", ".join(blockers))
    db.execute("DELETE FROM locks WHERE task_id = ?", (args.id,))
    db.execute("INSERT INTO retired(task_id, at, actor, reason, superseded_by) "
               "VALUES (?, ?, ?, ?, ?)",
               (args.id, now, args.owner, args.reason, args.superseded_by))
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "retire", args.id, args.owner, reason=args.reason,
          superseded_by=args.superseded_by, previous_status=row["status"])
    return {"task": task_json(db, args.id)}


def unretire_task(db, args, now):
    """Return a retired task to the backlog; the retirement stays in the audit trail."""
    task_row(db, args.id)
    record = retired_row(db, args.id)
    if not record:
        raise QueueError(f"not retired: {args.id}")
    db.execute("DELETE FROM retired WHERE task_id = ?", (args.id,))
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "unretire", args.id, args.owner, reason=args.reason, retirement=record)
    return {"task": task_json(db, args.id)}


def expire_claim(db, args, now):
    """End a live claim immediately.

    A worker that loses its token (a dead heartbeat process, a lost
    environment) otherwise has to idle until `lease_until` passes before it can
    claim its own task again. Expiring the claim here has exactly the effect
    the clock would have had: the task returns to `pending`, its locks are
    released and the old token stops working, so the next `claim` issues a
    fresh one. It never transfers authority silently: the owner must match and
    the reason is recorded in the audit trail. To keep the locks while
    recovering, use `reclaim` instead.
    """
    row = task_row(db, args.id)
    if row["status"] != "claimed":
        raise QueueError("expire requires a claimed task")
    if row["owner"] != args.owner:
        raise QueueError("expire requires the current owner of the claim")
    validate_revision_claim(db, args.id)
    make_pending(db, args.id, now)
    audit(db, now, "expire", args.id, args.owner, reason=args.reason,
          previous_owner=row["owner"], lease_until=row["lease_until"],
          administrative=True)
    return {"task": task_json(db, args.id)}


def update_claim(db, args, now):
    if args.command == 'submit' and args.id.startswith('integration-search-'):
        raise QueueError('integration searches are private search-only tasks and cannot be submitted')
    row = task_row(db, args.id)
    if (row["status"] != "claimed" or row["owner"] != args.owner
            or row["token"] != args.token or row["lease_until"] <= now):
        raise QueueError("requires a claimed task with matching owner, token and unexpired lease")
    revision = validate_revision_claim(db, args.id)
    if args.command == "heartbeat":
        until = now + args.lease_seconds
        db.execute("UPDATE tasks SET lease_until = ?, updated_at = ? WHERE id = ?",
                   (until, now, args.id))
        audit(db, now, "heartbeat", args.id, args.owner, lease_until=until)
    elif args.command == "submit":
        if revision:
            _path, data, _digest = file_evidence(args.artifact, 'successor artifact')
            try:
                submitted = json.loads(data)
                if (submitted.get('task_id') != args.id or
                        submitted.get('handoff_action') != 'submit' or
                        str(_path) == revision['predecessor_artifact']):
                    raise QueueError('successor must submit its own fresh result, not the predecessor artifact')
                revision_identity(submitted, revision, 'successor artifact')
            except (ValueError, TypeError, AttributeError) as exc:
                raise QueueError('successor artifact is invalid: ' + str(exc)) from exc
        # Submitted work has no lease and keeps its locks until review.
        submission_id = secrets.token_hex(32)
        db.execute("""UPDATE tasks SET status = 'submitted', token = ?,
                       lease_until = NULL, artifact = ?, updated_at = ? WHERE id = ?""",
                   (submission_id, args.artifact, now, args.id))
        audit(db, now, "submit", args.id, args.owner, artifact=args.artifact,
              submission_id=submission_id)
    else:
        make_pending(db, args.id, now)
        audit(db, now, "release", args.id, args.owner, note=args.note)
    return {"task": task_json(db, args.id)}


def reviewable(db, args, allow_submitted_revision=False):
    protect_revision_predecessor(db, args.id)
    if submitted_revision(db, args.id) and not allow_submitted_revision:
        raise QueueError('submitted matching successors have no review/acceptance transition in this API')
    row = task_row(db, args.id)
    if row["status"] != "submitted":
        raise QueueError("only submitted tasks can be reviewed")
    if row["token"] != args.submission_id:
        raise QueueError("submission changed; review the current submission before finishing")
    return row


def validate_ready_report(data, args, row, repo=None):
    """Bind a TU ready mark to this report's completed verdict and submission."""
    from review_stage import validate_report

    repo = Path(repo or Path(__file__).resolve().parents[1])
    try:
        report = json.loads(data)
    except (ValueError, UnicodeError) as exc:
        raise QueueError(f"--report is not a review report: {exc}") from exc
    errors, _ = validate_report(report, repo)
    if errors:
        raise QueueError(f"--report fails review-report validation: {errors[:3]}")
    matching = [(candidate, source) for candidate in report['candidates']
                for source in candidate.get('sources', [])
                if (source['task'], source['submission_id'])
                == (args.id, args.submission_id)]
    if len(matching) != 1:
        raise QueueError("--report must name this exact task/submission once")
    candidate, source = matching[0]
    if report['reviewer'] in (row['owner'], source.get('author'), source['task']):
        raise QueueError("the report's reviewer authored this submission")
    if candidate['verdict'] not in ('ready_to_integrate', 'accepted_with_adjustment'):
        raise QueueError(f"--report verdict {candidate['verdict']} is not ready; "
                         "withdraw an existing mark with unready on this submission")
    if candidate.get('review_complete') is not True:
        raise QueueError("--report review_complete is false")
    if not row['artifact']:
        raise QueueError("the current submission has no artifact")
    result = (repo / source['result']['path']).resolve()
    artifact = (repo / row['artifact']).resolve()
    if result != artifact:
        raise QueueError("--report result path differs from the submitted artifact")
    if (not result.is_file()
            or hashlib.sha256(result.read_bytes()).hexdigest() != source['result']['sha256']):
        raise QueueError("--report result is missing or changed since review")


def mark_ready(db, args, now):
    """Record a reviewer's `ready_to_integrate` verdict on one submission.

    The verdict used to live only in review reports, so integrators rebuilt the
    list by hand and some reviewers closed `accept` before anything was
    published. The mark sits beside the task, like a retirement: the task stays
    `submitted` and keeps its locks, the integrator lists ready tasks with
    `list --status ready_to_integrate`, and the publication step closes the
    submission with `finish --closed-by-integration` once it is published (the
    reviewer stays the accepting identity). A resubmission gets a new
    submission ID, so an old mark never applies to new source.
    A new report at another path does not supersede the queue mark: on a
    same-submission re-review, withdraw the old mark with `unready` before
    recording the new verdict. A completed ready verdict may then be re-marked;
    rework/reject stays unready (or uses the existing `finish --decision rework`).
    """
    row = reviewable(db, args)
    if args.reviewer == row["owner"]:
        raise QueueError("self review is not allowed")
    report, data, digest = file_evidence(args.report, "--report")
    if is_tu_candidate(db, args.id):
        validate_ready_report(data, args, row)
    ensure_ready_table(db, now, args.reviewer)
    if open_ready_row(db, args.id, args.submission_id):
        raise QueueError(f"{args.id} is already marked ready for this submission; "
                         "`unready` it first to replace the mark")
    db.execute("DELETE FROM integration_ready WHERE task_id = ? AND submission_id = ?",
               (args.id, args.submission_id))
    db.execute("INSERT INTO integration_ready(task_id, submission_id, at, reviewer, report, "
               "report_sha256, note) VALUES (?, ?, ?, ?, ?, ?, ?)",
               (args.id, args.submission_id, now, args.reviewer, str(report), digest, args.note))
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "ready", args.id, args.reviewer, submission_id=args.submission_id,
          report=str(report), report_sha256=digest, note=args.note,
          previous_owner=row["owner"], artifact=row["artifact"])
    return {"task": task_json(db, args.id)}


def close_ready(db, task_id, submission_id, now, actor, action, reason):
    if not has_table(db, "integration_ready"):
        return None
    record = open_ready_row(db, task_id, submission_id)
    if record:
        db.execute("UPDATE integration_ready SET closed_at = ?, closed_by = ?, closed_action = ?, "
                   "closed_reason = ? WHERE task_id = ? AND submission_id = ?",
                   (now, actor, action, reason, task_id, submission_id))
    return record


def unmark_ready(db, args, now):
    row = reviewable(db, args)
    record = close_ready(db, args.id, args.submission_id, now, args.reviewer, "unready", args.reason)
    if not record:
        raise QueueError(f"{args.id} is not marked ready for this submission")
    db.execute("UPDATE tasks SET updated_at = ? WHERE id = ?", (now, args.id))
    audit(db, now, "unready", args.id, args.reviewer, submission_id=args.submission_id,
          reason=args.reason, mark=record, previous_owner=row["owner"])
    return {"task": task_json(db, args.id)}


def is_tu_candidate(db, task_id):
    resources = [r[0] for r in db.execute(
        "SELECT resource FROM resources WHERE task_id = ?", (task_id,))]
    return (INTEGRATION_RESOURCE not in resources
            and any(r.startswith(TU_CANDIDATE_PREFIXES) for r in resources))


def integration_closure(db, args, row, now):
    """Check that a publication step may record the reviewer's accept of one submission.

    User direction 2026-09-18: the integrator closes the queue tasks of the
    candidates it published, instead of every reviewer being resumed to do it.
    The acceptance judgement stays the reviewer's: only a submission the
    reviewer marked `ready_to_integrate` can be closed this way, the accepting
    identity recorded is that reviewer, and the publication must be the live
    `tu-publication/1` record of the integration claim that published it, naming
    this task and submission with the very report the mark hashed. The caller
    must hold that claim (owner and token), and it must hold
    `baseline:integration`. Returns (mark, evidence) for the audit event.
    """
    task = args.closed_by_integration
    if args.decision != "accept":
        raise QueueError("--closed-by-integration only records an accept; a rework verdict is the "
                         "reviewer's own `finish --decision rework`")
    if getattr(args, "accept_without_publication", None) is not None:
        raise QueueError("--closed-by-integration never takes --accept-without-publication")
    if getattr(args, "publication", None) is None:
        raise QueueError("--closed-by-integration requires --publication (the batch's publication.json)")
    owner = getattr(args, "integration_owner", None)
    token = getattr(args, "integration_token", None)
    if not owner or not token:
        raise QueueError("--closed-by-integration requires --integration-owner and "
                         "--integration-token-file (the integration claim's own token)")
    if task == args.id:
        raise QueueError("a task cannot close itself as its own integration")
    integ = task_row(db, task)
    if (integ["status"] != "claimed" or integ["owner"] != owner or integ["token"] != token
            or integ["lease_until"] <= now):
        raise QueueError(f"integration task {task} is not a live claim of {owner} with this token")
    lock = db.execute("SELECT task_id FROM locks WHERE resource = ?", (INTEGRATION_RESOURCE,)).fetchone()
    if lock is None or lock["task_id"] != task:
        raise QueueError(f"integration task {task} does not hold the {INTEGRATION_RESOURCE} lock")
    if owner == row["owner"]:
        raise QueueError("the integration claim's owner is the candidate's author; an author never "
                         "closes its own submission")
    mark = open_ready_row(db, args.id, args.submission_id)
    if not mark:
        raise QueueError(f"{args.id} has no open ready_to_integrate mark for this submission; only a "
                         "reviewer's ready verdict can be closed by the publication step")
    if mark["reviewer"] != args.reviewer:
        raise QueueError(f"--reviewer {args.reviewer} is not the reviewer of the ready mark "
                         f"({mark['reviewer']})")
    path, data, digest = file_evidence(args.publication, "--publication")
    try:
        pub = json.loads(data)
    except ValueError:
        raise QueueError(f"publication {path} is not JSON") from None
    if not isinstance(pub, dict) or pub.get("schema") != "tu-publication/1":
        raise QueueError(f"publication {path} is not a tu-publication/1 record")
    if pub.get("dry_run") or pub.get("result") != "published":
        raise QueueError(f"publication {path} is not a live, successful publication "
                         f"(dry_run={pub.get('dry_run')}, result={pub.get('result')})")
    if (pub.get("locks") or {}).get("integration_task") != task:
        raise QueueError(f"publication {path} was not written under integration task {task}")
    report_sha = {r.get("path"): r.get("sha256") for r in pub.get("reports") or [] if isinstance(r, dict)}
    naming = [p for p in pub.get("published") or [] if isinstance(p, dict) and any(
        s.get("task") == args.id and s.get("submission_id") == args.submission_id
        for s in p.get("sources") or [])]
    if not naming:
        raise QueueError(f"publication {path} publishes no candidate of {args.id} submission "
                         f"{args.submission_id[:12]}")
    for item in naming:
        used = report_sha.get((item.get("review") or {}).get("report"))
        if used != mark["report_sha256"]:
            raise QueueError(f"publication {path} published {item.get('candidate')} from a report "
                             f"(sha256 {str(used)[:12]}) other than the ready mark's "
                             f"({mark['report_sha256'][:12]})")
    evidence = dict(publication=str(path), publication_sha256=digest, publication_names_submission=True,
                    closed_by_integration=dict(task=task, owner=owner,
                                               candidates=[p.get("candidate") for p in naming]),
                    accepting_reviewer=mark["reviewer"], ready_report_sha256=mark["report_sha256"])
    return mark, evidence


def finish_task(db, args, now):
    row = reviewable(db, args, allow_submitted_revision=bool(
        getattr(args, "closed_by_direct_acceptance", None)))
    if getattr(args, "complete_publication_task", False):
        if (args.decision != "accept" or not args.publication or args.reviewer or
                args.closed_by_integration or args.closed_by_direct_acceptance or
                args.accept_without_publication or args.integration_owner or
                args.integration_token_file or args.direct_user_authorization):
            raise QueueError("publication-task completion requires only --decision accept and "
                             "--publication, without a reviewer or source-acceptance override")
        path, data, digest = file_evidence(args.publication, "--publication")
        pub = json.loads(data)
        final = pub.get("final_verification") or {}
        if (pub.get("schema") != "tu-publication/1" or pub.get("dry_run") or
                pub.get("result") != "published" or
                (pub.get("locks") or {}).get("integration_task") != args.id or
                (pub.get("locks") or {}).get("owner") != row["owner"] or
                Path(row["artifact"] or "").resolve() != path.resolve() or
                not final.get("build_ok") or final.get("failures") or
                final.get("published_files_drift") or
                not final.get("checks") or any(final["checks"].values())):
            raise QueueError("submitted artifact is not this publisher's successful, finally verified "
                             "live publication")
        db.execute("DELETE FROM locks WHERE task_id = ?", (args.id,))
        db.execute("UPDATE tasks SET status='done', updated_at=? WHERE id=?", (now, args.id))
        audit(db, now, "complete_publication", args.id, row["owner"], note=args.note,
              submission_id=args.submission_id, artifact=row["artifact"],
              publication=str(path), publication_sha256=digest,
              source_review_performed=False, source_acceptance_performed=False)
        return {"task": task_json(db, args.id)}
    if getattr(args, "closed_by_direct_acceptance", None) is not None:
        revision = validate_revision_claim(db, args.id)
        if revision:
            _, successor_data, _ = file_evidence(row["artifact"] or "", "revision successor artifact")
            revision_identity(json.loads(successor_data), revision, "revision successor")
        task = args.closed_by_direct_acceptance
        if args.decision != "accept" or not args.publication or not args.direct_user_authorization:
            raise QueueError("direct acceptance requires --decision accept, --publication, and the explicit user directive")
        if getattr(args, "closed_by_integration", None) is not None:
            raise QueueError("direct acceptance and reviewed integration closure are mutually exclusive")
        owner, token = getattr(args, "integration_owner", None), getattr(args, "integration_token", None)
        if not owner or not token or task == args.id:
            raise QueueError("direct acceptance requires a distinct live integration claim")
        integ = task_row(db, task)
        if (integ["status"] != "claimed" or integ["owner"] != owner or integ["token"] != token or
                integ["lease_until"] <= now):
            raise QueueError("direct-acceptance integration claim is not live and token-bound")
        lock = db.execute("SELECT task_id FROM locks WHERE resource = ?", (INTEGRATION_RESOURCE,)).fetchone()
        if lock is None or lock["task_id"] != task:
            raise QueueError("direct-acceptance integration claim lacks baseline:integration")
        path, data, digest = file_evidence(args.publication, "--publication")
        pub = json.loads(data)
        if (pub.get("schema") != "tu-publication/1" or pub.get("dry_run") or pub.get("result") != "published" or
                (pub.get("locks") or {}).get("integration_task") != task):
            raise QueueError("publication is not a successful live direct integration under this claim")
        naming = [p for p in pub.get("published") or [] if isinstance(p, dict) and any(
            s.get("task") == args.id and s.get("submission_id") == args.submission_id
            for s in p.get("sources") or [])]
        if not naming or any((p.get("acceptance_basis") or {}).get("kind") !=
                             "user_authorized_review_waiver" or
                             (p.get("acceptance_basis") or {}).get("directive") != args.direct_user_authorization or
                             p.get("review") is not None for p in naming):
            raise QueueError("publication does not record this exact user-waived submission without reviewer verdict")
        report_sha = {r.get("path"): r.get("sha256") for r in pub.get("reports") or [] if isinstance(r, dict)}
        for published in naming:
            if published.get("report") not in report_sha:
                raise QueueError("publication report hash list omits the direct-acceptance report")
            cited = [s for s in published.get("sources") or []
                     if s.get("task") == args.id and s.get("submission_id") == args.submission_id]
            if not cited or any(Path(s.get("result", {}).get("path", "")).resolve() !=
                                Path(row["artifact"] or "").resolve() for s in cited):
                raise QueueError("publication does not cite the current submitted author result")
            for s in cited:
                result_path = Path(s["result"]["path"])
                if not result_path.is_file() or file_evidence(result_path, "author result")[2] != \
                        s["result"].get("sha256"):
                    raise QueueError("publication author result hash differs from the submitted artifact")
        if revision:
            predecessor = revision["predecessor_task"]
            reason = "Superseded by directly published immutable revision " + args.id
            retire_submitted_superseded(db, task_row(db, predecessor), args.id, owner, reason, now,
                dict(artifact_sha256=revision["predecessor_artifact_sha256"],
                     submitted_revision=dict(revision), publication=str(path), publication_sha256=digest))
        db.execute("DELETE FROM locks WHERE task_id = ?", (args.id,))
        db.execute("UPDATE tasks SET status='done', updated_at=? WHERE id=?", (now, args.id))
        evidence = dict(publication=str(path), publication_sha256=digest, publication_names_submission=True,
                        closed_by_direct_acceptance=dict(task=task, owner=owner,
                                                         candidates=[p.get("candidate") for p in naming]),
                        user_direct_authorization=args.direct_user_authorization)
        if revision:
            evidence["submitted_revision"] = dict(revision)
        audit(db, now, "finish", args.id, f"user-authorized direct acceptance via {task}",
              decision="accept", note=args.note, previous_owner=row["owner"], artifact=row["artifact"],
              submission_id=args.submission_id, tu_candidate=is_tu_candidate(db, args.id), **evidence)
        return {"task": task_json(db, args.id)}
    if not args.reviewer:
        raise QueueError("--reviewer is required for reviewed finish decisions")
    if args.decision == "accept" and args.reviewer == row["owner"]:
        raise QueueError("self acceptance is not allowed")
    if getattr(args, "closed_by_integration", None) is not None:
        _, evidence = integration_closure(db, args, row, now)
        db.execute("DELETE FROM locks WHERE task_id = ?", (args.id,))
        db.execute("UPDATE tasks SET status = 'done', updated_at = ? WHERE id = ?", (now, args.id))
        mark = close_ready(db, args.id, args.submission_id, now,
                           f"{args.integration_owner} (integration {args.closed_by_integration})",
                           "finish-accept", args.note)
        # The actor is the accepting reviewer: the verdict is theirs; the
        # integration only records that the reviewed submission was published.
        audit(db, now, "finish", args.id, args.reviewer, decision="accept", note=args.note,
              previous_owner=row["owner"], artifact=row["artifact"],
              submission_id=args.submission_id, tu_candidate=is_tu_candidate(db, args.id),
              ready_mark=mark, **evidence)
        return {"task": task_json(db, args.id)}
    for name in ("integration_owner", "integration_token_file"):
        if getattr(args, name, None) is not None:
            raise QueueError(f"--{name.replace('_', '-')} only applies with --closed-by-integration")
    publication = getattr(args, "publication", None)
    override = getattr(args, "accept_without_publication", None)
    evidence = {}
    if override is not None and args.decision != "accept":
        raise QueueError("--accept-without-publication only applies to --decision accept")
    if publication is not None:
        path, data, digest = file_evidence(publication, "--publication")
        if args.id.encode() not in data:
            raise QueueError(f"publication evidence {path} does not name task {args.id}; cite the "
                             "publication record of this batch")
        evidence.update(publication=str(path), publication_sha256=digest,
                        publication_names_submission=args.submission_id.encode() in data)
    tu_candidate = is_tu_candidate(db, args.id)
    if args.decision == "accept" and tu_candidate and publication is None:
        if override is None:
            raise QueueError(
                "a TU candidate is accepted only after publication: pass --publication PATH "
                "(the integrator's record naming this task), or --accept-without-publication "
                "REASON for an audited exception. Before publication record the verdict with "
                "`ready` instead")
        evidence.update(publication_override=override)
    if args.decision == "rework":
        make_pending(db, args.id, now)
    else:
        db.execute("DELETE FROM locks WHERE task_id = ?", (args.id,))
        db.execute("UPDATE tasks SET status = 'done', updated_at = ? WHERE id = ?",
                   (now, args.id))
    mark = close_ready(db, args.id, args.submission_id, now, args.reviewer,
                       f"finish-{args.decision}", args.note)
    audit(db, now, "finish", args.id, args.reviewer, decision=args.decision,
          note=args.note, previous_owner=row["owner"], artifact=row["artifact"],
          submission_id=args.submission_id,
          **({"tu_candidate": True} if tu_candidate else {}),
          **({"ready_mark": mark} if mark else {}), **evidence)
    return {"task": task_json(db, args.id)}


def list_tasks(db, args):
    wanted = {"retired": None, "ready_to_integrate": "submitted"}.get(args.status, args.status)
    rows = db.execute("SELECT id FROM tasks WHERE (? IS NULL OR status = ?) "
                      "ORDER BY priority DESC, created_at ASC, id ASC",
                      (wanted, wanted)).fetchall()
    tasks = [task_json(db, row["id"]) for row in rows]
    if args.status == "retired":
        tasks = [task for task in tasks if task["retired"]]
    elif not args.include_retired:
        # Retired tasks are not work; they stay out of the backlog view
        # unless asked for by name.
        tasks = [task for task in tasks if not task["retired"]]
    if args.status == "ready_to_integrate":
        tasks = [task for task in tasks if task["ready_to_integrate"]]
    return {"tasks": tasks, "retired_hidden":
            not args.include_retired and args.status != "retired"}


def execute(args):
    with closing(connect(args.db, create=args.command == "init")) as db, db:
        indexes = ensure_indexes(db, any_size=True) if args.command == "init" else []
        db.execute("BEGIN" if args.command in ("list", "history") else "BEGIN IMMEDIATE")
        # Read the clock after acquiring the write lock, not before waiting for it.
        now = time.time()
        version = db.execute("PRAGMA user_version").fetchone()[0]
        if args.command == "init":
            added = []
            if version == 0:
                if db.execute("SELECT 1 FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'").fetchone():
                    raise QueueError("refusing to initialize a nonempty, unrecognized database")
                for statement in SCHEMA + RETIRED_SCHEMA + READY_SCHEMA + (SUBMITTED_REVISION_SCHEMA,):
                    db.execute(statement)
                db.execute(f"PRAGMA user_version = {VERSION}")
                audit(db, now, "init")
            elif version == 1:
                # Version 2 is additive: create the one new table and bump.
                for statement in RETIRED_SCHEMA:
                    db.execute(statement)
                db.execute(f"PRAGMA user_version = {VERSION}")
                audit(db, now, "migrate", None, None, to_version=VERSION, added=["retired"])
                added.append("retired")
            elif version != VERSION:
                raise QueueError(f"unsupported database version: {version}")
            # The ready table is additive within version 2 (see READY_SCHEMA).
            if version != 0 and ensure_ready_table(db, now):
                added.append("integration_ready")
            if version != 0 and not has_table(db, 'submitted_revisions'):
                db.execute(SUBMITTED_REVISION_SCHEMA)
                added.append('submitted_revisions')
                audit(db, now, 'migrate-submitted-revisions')
            # Verify the expected tables even on an idempotent init.
            for table in ("tasks", "resources", "dependencies", "locks", "audit", "retired",
                          "integration_ready", "submitted_revisions"):
                db.execute(f"SELECT 1 FROM {table} LIMIT 1")
            if version == 0:
                # A brand-new queue: its (empty) indexes are part of the schema.
                for _, statement in INDEXES:
                    db.execute(statement)
            return {"initialized": True, "db": str(Path(args.db)), "version": VERSION,
                    "added": added, "indexes_created": indexes}
        if version not in (1, VERSION):
            raise QueueError("database is not an initialized workqueue "
                             f"(expected version 1 or {VERSION})")
        if version < VERSION and args.command in ("retire", "unretire", "ready", "unready"):
            raise QueueError("this queue predates the retired table; run `init` once to migrate")
        if args.command == "add":
            return add_task(db, args, now)
        if args.command == "claim":
            return claim_task(db, args, now)
        if args.command == 'revise-submitted':
            return revise_submitted_claim(db, args, now)
        if args.command in ("heartbeat", "submit", "release"):
            return update_claim(db, args, now)
        if args.command == "narrow":
            return narrow_claim(db, args, now)
        if args.command == "narrow-submitted":
            return narrow_submitted(db, args, now)
        if args.command == "reclaim":
            return reclaim_claim(db, args, now)
        if args.command == "retire":
            return retire_task(db, args, now)
        if args.command == "unretire":
            return unretire_task(db, args, now)
        if args.command == "expire":
            return expire_claim(db, args, now)
        if args.command == "ready":
            return mark_ready(db, args, now)
        if args.command == "unready":
            return unmark_ready(db, args, now)
        if args.command == "finish":
            return finish_task(db, args, now)
        if args.command == "history":
            task_row(db, args.id)
            events = []
            for row in db.execute("SELECT * FROM audit WHERE task_id = ? ORDER BY id", (args.id,)):
                event = dict(row)
                event["detail"] = json.loads(event["detail"])
                events.append(event)
            return {"events": events}
        return list_tasks(db, args)


def is_busy(error):
    return (isinstance(error, sqlite3.OperationalError)
            and ("locked" in str(error) or "busy" in str(error)))


def execute_with_retry(args, budget):
    """Run one command, retrying only a lock refusal (which committed nothing)."""
    deadline = time.monotonic() + budget
    for attempt in range(1 << 30):
        try:
            return execute(args)
        except sqlite3.OperationalError as error:
            delay = BACKOFF[min(attempt, len(BACKOFF) - 1)]
            if not is_busy(error) or time.monotonic() + delay > deadline:
                raise
            time.sleep(delay)


# --------------------------------------------------------------------------
# Heartbeat daemon
# --------------------------------------------------------------------------

def daemon_paths(args):
    token_path = private_token_path(args.token_file)
    paths = {"token_file": token_path}
    for name in ("pidfile", "stopfile", "log"):
        value = getattr(args, name)
        suffix = {"pidfile": "pid", "stopfile": "stop", "log": "log"}[name]
        paths[name] = (private_token_path(value) if value
                       else token_path.parent / f"{token_path.name}.heartbeat.{suffix}")
    paths["failed"] = paths["pidfile"].with_name(paths["pidfile"].name + ".failed")
    return paths


def process_start(pid):
    """Kernel start time of a process, so a recycled PID never reads as our daemon."""
    try:
        text = Path(f"/proc/{pid}/stat").read_text(encoding="ascii", errors="replace")
    except OSError:
        return None
    fields = text[text.rindex(")") + 2:].split()
    return fields[19] if len(fields) > 19 else None


def pid_alive(pidfile):
    """The daemon PID recorded in a pidfile if that very process still runs."""
    try:
        pid_text, start = pidfile.read_text(encoding="ascii").split()[:2]
        pid = int(pid_text)
    except (OSError, ValueError):
        return None
    return pid if start == process_start(pid) else None


def log_line(path, message):
    with open(path, "a", encoding="utf-8") as stream:
        stream.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')} {message}\n")


def claim_state(args):
    """Why a heartbeat was refused: 'ended' (never retry) or 'live'."""
    with closing(connect(args.db, create=False)) as db:
        row = db.execute("SELECT status, owner, token, lease_until FROM tasks WHERE id = ?",
                         (args.id,)).fetchone()
    if row is None or row["status"] != "claimed" or row["owner"] != args.owner:
        return "ended", (row["status"] if row else "unknown task")
    if row["token"] != args.token:
        return "ended", "token rotated or replaced"
    if row["lease_until"] <= time.time():
        return "ended", "lease lapsed"
    return "live", "claimed"


def run_daemon(args, paths):
    """The detached renewer loop. Never logs the token."""
    stop = {"flag": False}
    signal.signal(signal.SIGTERM, lambda *_: stop.update(flag=True))
    log = paths["log"]
    started = time.monotonic()
    renewed_until = getattr(args, "renewed_until", None)
    code = 0
    log_line(log, f"started pid={os.getpid()} task={args.id} interval={args.interval}s "
                  f"lease={args.lease_seconds}s max={args.max_seconds}s")
    try:
        while True:
            if stop["flag"] or paths["stopfile"].exists():
                log_line(log, "stopped: stop requested")
                break
            if time.monotonic() - started > args.max_seconds:
                log_line(log, "stopped: --max-seconds reached")
                break
            if not paths["token_file"].exists():
                log_line(log, "stopped: token file removed")
                break
            attempt = 0
            while True:
                try:
                    # Re-read each beat: `reclaim` rotates the token in place.
                    args.token = read_token_file(str(paths["token_file"]))
                    result = execute(args)
                    renewed_until = result["task"]["lease_until"]
                    log_line(log, f"renewed lease_until={renewed_until:.0f}")
                    break
                except (sqlite3.Error, OSError, QueueError) as error:
                    state, why = ("live", str(error))
                    if isinstance(error, QueueError):
                        try:
                            state, why = claim_state(args)
                        except (sqlite3.Error, OSError) as inner:
                            state, why = "live", str(inner)
                    if state == "ended":
                        log_line(log, f"stopped: claim ended ({why})")
                        raise DaemonStop
                    delay = BACKOFF[min(attempt, len(BACKOFF) - 1)]
                    attempt += 1
                    limit = (renewed_until or 0) - DAEMON_SAFETY_MARGIN
                    if renewed_until is not None and time.time() + delay > limit:
                        log_line(log, f"FAILED: renewal kept failing ({why}); lease at risk")
                        paths["failed"].write_text(f"{time.time():.0f} {why}\n", encoding="utf-8")
                        code = 3
                        raise DaemonStop
                    if renewed_until is None and attempt > len(BACKOFF):
                        log_line(log, f"FAILED: first renewal kept failing ({why})")
                        paths["failed"].write_text(f"{time.time():.0f} {why}\n", encoding="utf-8")
                        code = 3
                        raise DaemonStop
                    log_line(log, f"retry in {delay}s: {why}")
                    for _ in range(delay):
                        if stop["flag"] or paths["stopfile"].exists():
                            break
                        time.sleep(1)
                    if stop["flag"] or paths["stopfile"].exists():
                        raise DaemonStop
            for _ in range(args.interval):
                if stop["flag"] or paths["stopfile"].exists():
                    break
                time.sleep(1)
    except DaemonStop:
        pass
    finally:
        if pid_alive(paths["pidfile"]) == os.getpid():
            try:
                paths["pidfile"].unlink()
            except OSError:
                pass
    return code


def start_daemon(args):
    if args.interval >= args.lease_seconds:
        raise QueueError("--interval must be shorter than --lease-seconds")
    paths = daemon_paths(args)
    # Fail in the foreground on a bad token file or a refused first renewal.
    args.token = read_token_file(args.token_file)
    running = pid_alive(paths["pidfile"])
    if running:
        raise QueueError(f"a heartbeat daemon is already running for this token file (pid {running})")
    args.renewed_until = execute_with_retry(args, 60)["task"]["lease_until"]
    for name in ("stopfile", "failed"):
        try:
            paths[name].unlink()
        except FileNotFoundError:
            pass
    reader, writer = os.pipe()
    first = os.fork()
    if first:
        os.close(writer)
        with os.fdopen(reader, "r") as stream:
            message = stream.read()
        os.waitpid(first, 0)
        if not message.startswith("ok "):
            raise QueueError(f"heartbeat daemon did not start: {message.strip() or 'no report'}")
        return {"daemon": int(message.split()[1]), "task": args.id,
                **{name: str(path) for name, path in paths.items() if name != "token_file"}}
    # First child: new session, then fork again so the daemon is reparented.
    os.close(reader)
    os.setsid()
    if os.fork():
        os._exit(0)
    code = 1
    try:
        os.chdir("/")
        handle = os.open(paths["pidfile"], os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(handle, "w", encoding="ascii") as stream:
            stream.write(f"{os.getpid()} {process_start(os.getpid())}\n")
        null = os.open(os.devnull, os.O_RDWR)
        logfd = os.open(paths["log"], os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
        os.dup2(null, 0)
        os.dup2(logfd, 1)
        os.dup2(logfd, 2)
        os.write(writer, f"ok {os.getpid()}\n".encode())
        os.close(writer)
        writer = None
        code = run_daemon(args, paths)
    except BaseException as error:  # report to the parent, never raise in a fork
        if writer is not None:
            try:
                os.write(writer, f"error {error}\n".encode())
            except OSError:
                pass
    finally:
        os._exit(code)


def stop_daemon(args):
    paths = daemon_paths(args)
    paths["stopfile"].touch(mode=0o600)
    pid = pid_alive(paths["pidfile"])
    if pid:
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError as error:
            if error.errno != errno.ESRCH:
                raise
        for _ in range(50):
            if not pid_alive(paths["pidfile"]):
                break
            time.sleep(0.1)
    return {"stopped": pid, "still_running": pid_alive(paths["pidfile"]),
            "stopfile": str(paths["stopfile"]), "log": str(paths["log"])}


def main(argv=None):
    try:
        args = parser().parse_args(argv)
        if args.command == "heartbeat" and (args.daemon or args.stop):
            if not args.token_file:
                raise QueueError("--daemon/--stop require --token-file (a token on the "
                                 "command line of a long-lived process is visible to every user)")
            result = stop_daemon(args) if args.stop else start_daemon(args)
        else:
            if getattr(args, "token_file", None) and args.command not in (
                    'claim', 'reclaim', 'revise-submitted'):
                args.token = read_token_file(args.token_file)
            if getattr(args, "integration_token_file", None):
                args.integration_token = read_token_file(args.integration_token_file)
            result = execute_with_retry(args, args.busy_retry)
    except (QueueError, sqlite3.Error, OSError, ValueError) as error:
        print(json.dumps({"error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
