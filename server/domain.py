"""Domain validation for atomic desktop transactions. No database or HTTP dependencies."""
import copy
import re
from datetime import datetime

TABLES = {
    'loans': {'bell_period', 'grade_level', 'teacher_name', 'recipient_name', 'laptop_number', 'initial_condition', 'return_condition', 'returner_name', 'lend_time', 'return_time', 'status', 'notes'},
    'preset_teachers': {'name'}, 'preset_grades': {'name'},
    'stock_types': {'name', 'brand', 'model', 'description'},
    'inventory_items': {'type_id', 'name', 'brand', 'model', 'code', 'description', 'location', 'total', 'low_stock_threshold'},
    'inventory_loans': {'item_id', 'borrower', 'quantity', 'returned', 'missing', 'lend_time', 'due_time', 'last_return_time', 'notes'},
    'inventory_missing': {'item_id', 'loan_id', 'quantity', 'recovered', 'reporter', 'notes', 'reported_time'},
    'inventory_events': {'item_id', 'action', 'quantity', 'person', 'notes', 'time'},
}
REFERENCES = {'inventory_items': {'type_id': 'stock_types'}, 'inventory_loans': {'item_id': 'inventory_items'},
              'inventory_missing': {'item_id': 'inventory_items', 'loan_id': 'inventory_loans'},
              'inventory_events': {'item_id': 'inventory_items'}}
COUNTERS = {'inventory_loans': {'returned', 'missing'}, 'inventory_missing': {'recovered'}}
STAFF_UPDATES = {'loans': TABLES['loans'], 'inventory_loans': {'returned', 'missing', 'last_return_time'},
                 'inventory_missing': {'recovered'}}
ID_PATTERN = re.compile(r'^[0-9a-f]{32}$')

class Conflict(Exception):
    def __init__(self, code, message):
        self.code, self.message = code, message
        super().__init__(message)

def identity(value):
    if not isinstance(value, str) or not ID_PATTERN.fullmatch(value):
        raise Conflict('INVALID_ID', 'شناسه رکورد معتبر نیست.')
    return value

def integer(value, minimum=0, maximum=1000000):
    if type(value) is not int or not minimum <= value <= maximum:
        raise Conflict('INVALID_QUANTITY', 'تعداد وارد شده معتبر نیست.')
    return value

def text(value, required=False, maximum=20000):
    if value is None and not required:
        return ''
    if not isinstance(value, str) or len(value) > maximum or (required and not value.strip()):
        raise Conflict('INVALID_TEXT', 'متن الزامی یا طول متن معتبر نیست.')
    return value

def date(value, required=False):
    if not value and not required:
        return
    try:
        datetime.fromisoformat(value.replace('Z', '+00:00'))
    except (TypeError, ValueError, AttributeError):
        raise Conflict('INVALID_DATE', 'تاریخ معتبر نیست.')

def normalize_laptop(value):
    return value.strip().casefold().translate(str.maketrans('۰۱۲۳۴۵۶۷۸۹٠١٢٣٤٥٦٧٨٩', '01234567890123456789'))

def validate(state):
    """Check the prospective complete state before committing any part of a batch."""
    active = set()
    unique = {name: set() for name in ['stock_types', 'preset_teachers', 'preset_grades', 'inventory_items']}
    reserved = {}
    for (table, entity_id), row in state.items():
        identity(entity_id)
        if table not in TABLES or not isinstance(row, dict) or set(row) != TABLES[table]:
            raise Conflict('INVALID_FIELDS', 'فیلدهای رکورد معتبر نیستند.')
        for key, value in row.items():
            if key not in {'bell_period', 'quantity', 'returned', 'missing', 'recovered', 'total', 'low_stock_threshold'}:
                if key in REFERENCES.get(table, {}):
                    if value:
                        identity(value)
                        if (REFERENCES[table][key], value) not in state:
                            raise Conflict('MISSING_REFERENCE', 'رکورد وابسته حذف شده یا هنوز ثبت نشده است.')
                    elif key != 'loan_id':
                        raise Conflict('MISSING_REFERENCE', 'رکورد وابسته الزامی است.')
                else:
                    text(value)
        if table == 'loans':
            integer(row['bell_period'], 1, 5)
            for field in ['teacher_name', 'recipient_name', 'laptop_number', 'initial_condition']:
                text(row[field], True)
            date(row['lend_time'], True)
            if row['status'] not in {'ACTIVE', 'RETURNED'}:
                raise Conflict('INVALID_STATUS', 'وضعیت امانت معتبر نیست.')
            if row['status'] == 'ACTIVE':
                number = normalize_laptop(row['laptop_number'])
                if number in active:
                    raise Conflict('LAPTOP_BUSY', 'این لپ‌تاپ در رکورد فعال دیگری در امانت است.')
                active.add(number)
                if row['return_time'] or row['returner_name'] or row['return_condition']:
                    raise Conflict('INVALID_RETURN', 'امانت فعال نباید اطلاعات بازگشت داشته باشد.')
            else:
                date(row['return_time'], True)
                text(row['returner_name'], True); text(row['return_condition'], True)
        if table in {'stock_types', 'preset_teachers', 'preset_grades'}:
            name = text(row['name'], True).strip()
            if name in unique[table]:
                raise Conflict('DUPLICATE_NAME', 'این نام قبلاً ثبت شده است.')
            unique[table].add(name)
        if table == 'inventory_items':
            text(row['name'], True)
            integer(row['total']); integer(row['low_stock_threshold'])
            if row['code']:
                if row['code'] in unique[table]:
                    raise Conflict('DUPLICATE_CODE', 'کد کالا باید یکتا باشد.')
                unique[table].add(row['code'])
        if table == 'inventory_loans':
            integer(row['quantity'], 1); integer(row['returned']); integer(row['missing'])
            text(row['borrower'], True); date(row['lend_time'], True); date(row['due_time']); date(row['last_return_time'])
            outstanding = row['quantity'] - row['returned'] - row['missing']
            if outstanding < 0:
                raise Conflict('OVER_RETURN', 'تعداد بازگشت یا مفقودی از تعداد در امانت بیشتر است.')
            reserved[row['item_id']] = reserved.get(row['item_id'], 0) + outstanding
        if table == 'inventory_missing':
            integer(row['quantity'], 1); integer(row['recovered'])
            text(row['reporter'], True); text(row['notes'], True); date(row['reported_time'], True)
            if row['recovered'] > row['quantity']:
                raise Conflict('OVER_RECOVERY', 'تعداد پیدا شده از مفقودی بیشتر است.')
            if row['loan_id'] and state[('inventory_loans', row['loan_id'])]['item_id'] != row['item_id']:
                raise Conflict('WRONG_LOAN', 'امانت مربوط به این کالا نیست.')
            reserved[row['item_id']] = reserved.get(row['item_id'], 0) + row['quantity'] - row['recovered']
        if table == 'inventory_events':
            integer(row['quantity'], -1000000)
            if row['action'] not in {'ADD', 'EDIT', 'LEND', 'RETURN', 'MISSING', 'FOUND'}:
                raise Conflict('INVALID_EVENT', 'نوع گردش انبار معتبر نیست.')
            date(row['time'], True)
    for item, quantity in reserved.items():
        if quantity > state[('inventory_items', item)]['total']:
            raise Conflict('INSUFFICIENT_STOCK', 'موجودی مشترک کافی نیست؛ کاربر دیگری از این کالا استفاده کرده است.')
    # Loan losses must have corresponding reports; recovery does not reopen the old loan.
    reported = {}
    for (table, _), row in state.items():
        if table == 'inventory_missing' and row['loan_id']:
            reported[row['loan_id']] = reported.get(row['loan_id'], 0) + row['quantity']
    for (table, entity_id), row in state.items():
        if table == 'inventory_loans' and row['missing'] != reported.get(entity_id, 0):
            raise Conflict('MISSING_REPORT', 'مفقودی امانت باید همراه با گزارش ثبت شود.')


def apply_entries(state, versions, entries, role, expected_revision):
    candidate = copy.deepcopy(state)
    touched = set()
    if not entries or len(entries) > 100:
        raise Conflict('INVALID_BATCH', 'تعداد تغییرات معتبر نیست.')
    for entry in entries:
        table = entry.get('table'); entity_id = identity(entry.get('entity_id'))
        if table not in TABLES:
            raise Conflict('INVALID_TABLE', 'نوع رکورد معتبر نیست.')
        key = (table, entity_id)
        if key in touched:
            raise Conflict('DUPLICATE_ENTRY', 'یک رکورد نباید در یک تراکنش چند بار تغییر کند.')
        touched.add(key)
        before, after = entry.get('before'), entry.get('after')
        if before is None and after is None:
            raise Conflict('INVALID_BATCH', 'تغییر خالی معتبر نیست.')
        if before is not None and (not isinstance(before, dict) or set(before) != TABLES[table]):
            raise Conflict('INVALID_FIELDS', 'نسخه قبلی رکورد معتبر نیست.')
        if after is not None and (not isinstance(after, dict) or set(after) != TABLES[table]):
            raise Conflict('INVALID_FIELDS', 'نسخه جدید رکورد معتبر نیست.')
        changed = set(after or {}) if before is None else {field for field in TABLES[table] if (after or {}).get(field) != before.get(field)}
        if role == 'viewer':
            raise Conflict('FORBIDDEN', 'حساب شما فقط اجازه مشاهده دارد.')
        if role == 'staff':
            allowed_create = {'loans', 'inventory_loans', 'inventory_missing', 'inventory_events'}
            if after is None or (before is None and table not in allowed_create) or (before is not None and not changed <= STAFF_UPDATES.get(table, set())):
                raise Conflict('FORBIDDEN', 'این تغییر به دسترسی مدیر نیاز دارد.')
        if table == 'inventory_events' and before is not None:
            raise Conflict('IMMUTABLE_EVENT', 'گردش انبار قابل ویرایش یا حذف نیست.')
        if table in {'inventory_loans', 'inventory_missing'} and before is not None:
            allowed = COUNTERS[table] | ({'last_return_time'} if table == 'inventory_loans' else set())
            if after is None or not changed <= allowed:
                raise Conflict('IMMUTABLE_LOAN', 'مشخصات اصلی امانت و گزارش قابل تغییر نیستند.')
        if before is None:
            if key in candidate or versions.get(key, 0):
                raise Conflict('ALREADY_EXISTS', 'شناسه رکورد قبلاً ثبت شده است.')
            candidate[key] = after
            continue
        if key not in candidate:
            raise Conflict('DELETED', 'این رکورد توسط کاربر دیگری حذف شده است.')
        current = candidate[key]
        counter_update = table in COUNTERS and changed <= (COUNTERS[table] | {'last_return_time'})
        if not counter_update and versions.get(key, 0) != expected_revision(entry, key):
            raise Conflict('STALE_REVISION', 'رکورد توسط کاربر دیگری تغییر کرده است؛ هر دو نسخه برای بررسی حفظ شدند.')
        if after is None:
            del candidate[key]
        elif counter_update:
            updated = copy.deepcopy(current)
            for field in changed:
                if field in COUNTERS[table]:
                    delta = integer(after[field]) - integer(before[field])
                    if delta <= 0:
                        raise Conflict('INVALID_DELTA', 'تغییر تعداد باید افزایشی باشد.')
                    updated[field] = integer(current[field]) + delta
                else:
                    updated[field] = after[field]
            candidate[key] = updated
        else:
            # Patch changed fields, rather than replacing fields the user never edited.
            candidate[key] = {**current, **{field: after[field] for field in changed}}
    validate(candidate)
    return candidate, touched
