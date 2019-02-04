import re

from behave import given, when, then

@then('gpstate output looks like')
def impl(context):
    # Check the header line first.
    header_pattern = r'[ \t]+'.join(context.table.headings)
    check_stdout_msg_in_order(context, header_pattern)

    check_rows_exist(context)


@then('gpstate output has rows')
def impl(context):
    check_rows_exist(context)


@then('gpstate output has rows with keys values')
def impl(context):
    # Check that every row exists in the standard out in the specified order.
    # We accept any amount of horizontal whitespace in between columns.
    def check_row(row):
        split_row = map(lambda str: str.strip(), ''.join(row).split('='))
        row_pattern = r'[ \t]+=[ \t]+'.join(split_row)
        check_stdout_msg_in_order(context, row_pattern)

    check_row(context.table.headings)

    for row in context.table:
        check_row(row)


def check_rows_exist(context):
    # Check that every row exists in the standard out. We accept any amount
    # of horizontal whitespace in between columns.
    for row in context.table:
        row_pattern = r'[ \t]+'.join(row)
        check_stdout_msg_in_order(context, row_pattern)

def check_stdout_msg_in_order(context, msg):
    """
    Searches forward in context.stdout_message for a string matching the msg
    pattern. Once output has been matched, it's no longer considered for future
    matching. Use this matcher for order-dependent output tests.
    """
    # Lazily initialize the stdout_position -- if this is the first time we've
    # called this, start at the beginning.
    if 'stdout_position' not in context:
        context.stdout_position = 0

    pat = re.compile(msg)

    match = pat.search(context.stdout_message, pos=context.stdout_position)
    if not match:
        err_str = (
            "Expected stdout string '%s' in remaining output:\n"
            "%s\n\n"
            "Full output was\n%s"
        ) % (msg, context.stdout_message[context.stdout_position:], context.stdout_message)
        raise Exception(err_str)

    context.stdout_position = match.end()
