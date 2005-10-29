#!/bin/sh
set -x

> /usr/spool/mail/test.local
echo "Forward to test.local" > /usr/spool/mail/test.forward
echo "Pipe to cat > /tmp/test.mail" > /usr/spool/mail/test.pipe
chmod 644 /usr/spool/mail/test.pipe

mail test.local <<EOF
mailed to test.local
EOF
mail test.forward <<EOF
mailed to test.forward
EOF
mail test.pipe <<EOF
mailed to test.pipe
EOF
mail dutoit!bowell!test.local <<EOF
mailed to dutoit!bowell!test.local
EOF

sleep 60

ls -l /usr/spool/mail/test.*
ls -l /tmp/test.mail
echo ">>>test.local<<<"
cat /usr/spool/mail/test.local
echo ">>>test.mail<<<"
cat /tmp/test.mail
