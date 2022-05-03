# This code supports verifying group implementations which have branches
# or conditional statements (like cmovs), by allowing each execution path
# to independently set assumptions on input or intermediary variables.
#
# The general approach is:
# * A constraint is a tuple of two sets of symbolic expressions:
#   the first of which are required to evaluate to zero, the second of which
#   are required to evaluate to nonzero.
#   - A constraint is said to be conflicting if any of its nonzero expressions
#     is in the ideal with basis the zero expressions (in other words: when the
#     zero expressions imply that one of the nonzero expressions are zero).
# * There is a list of laws that describe the intended behaviour, including
#   laws for addition and doubling. Each law is called with the symbolic point
#   coordinates as arguments, and returns:
# 