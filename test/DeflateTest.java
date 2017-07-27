import java.io.Closeable;
import java.util.concurrent.ThreadLocalRandom;
import java.util.zip.DataFormatException;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

class SetThreadName
        implements Closeable
{
    private final String originalThreadName;

    public SetThreadName(String name)
    {
        originalThreadName = Thread.currentThread().getName();
        Thread.currentThread().setName(name);
    }

    @Override
    public void close()
    {
        Thread.currentThread().setName(originalThreadName);
    }
}

public class DeflateTest
{
    public static void main(String[] args)
            throws DataFormatException, InterruptedException
    {
        byte[] in = new byte[100_000_000];
        byte[] out = new byte[2 * in.length];
        
        while (true) {
            try(SetThreadName ignored = new SetThreadName("deflate-thread")){
                ThreadLocalRandom.current().nextBytes(in);
                Deflater deflater = new Deflater();
                deflater.setInput(in);
                deflater.finish();
                deflater.deflate(out);
                Thread.sleep(1000);
            }

            try(SetThreadName ignored = new SetThreadName("inflate-thread")){
                Inflater inflater = new Inflater();
                inflater.setInput(out);
                inflater.inflate(in);
            }
        }
    }
}
