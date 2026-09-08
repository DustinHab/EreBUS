/* sum a range, split across machines, combined by sum.
 * a split piece arrives as a RANG message: buf[2] = low, buf[3] = high;
 * the answer is one "TEXT" message, and a run of digits is read as a number. */
long main(long console, long inbox)
{
    long buf[16];
    long lo, hi, s, i;
    syscall(3, inbox, buf, 0, 0, 0);
    lo = buf[2];
    hi = buf[3];
    s = 0;
    for (i = lo; i <= hi; i = i + 1)
        s = s + i;
    syscall(2, console, 0x54584554, s, 0, 0);
    return 0;
}
