using System;

namespace Frost
{
    public class RuntimeException
    {

        public static void OnException(object exception)
        {
            Console.WriteLine("RunTimeException.OnException");
            if(exception != null)
            {
                if(exception is NullReferenceException)
                {
                    var e = exception as NullReferenceException;
                    Console.WriteLine(e.Message);
                    Console.WriteLine(e.Source);
                    Console.WriteLine(e.StackTrace);
                }
            }
            else if (exception is Exception)
            {
                var e = exception as Exception;
                Console.WriteLine(e.Message);
                Console.WriteLine(e.Source);
                Console.WriteLine(e.StackTrace);
            }
            else
            {
                Console.WriteLine("Exception is null");
            }
        }

    }
}
