
//https://www.gitmemory.com/melanchall
var eventsReceived = new List<MidiEvent>();

using (var outputDevice = OutputDevice.GetByName("MIDI A"))
{
    outputDevice.PrepareForEventsSending();

    using (var inputDevice = InputDevice.GetByName("MIDI A"))
    {
        inputDevice.EventReceived += (_, e) => eventsReceived.Add(e.Event);
        inputDevice.StartEventsListening();

        outputDevice.SendEvent(new NormalSysExEvent(new byte[] { 0x7F, 0x7F, 0x06, 0x03, 0xF7 }));
        outputDevice.SendEvent(new NormalSysExEvent(new byte[] { 0x7F, 0x7F, 0x06, 0x01, 0xF7 }));
        outputDevice.SendEvent(new NormalSysExEvent(new byte[] { 0x7F, 0x7F, 0x06, 0x44, 0x06, 0x01, 0x21, 0x02, 0x00, 0x00, 0x00, 0xF7 }));
        outputDevice.SendEvent(new NormalSysExEvent(new byte[] { 0x7F, 0x7F, 0x06, 0x40, 0x03, 0x46, 0x01, 0x0A, 0xF7 }));

        SpinWait.SpinUntil(() => eventsReceived.Count == 4);

        Debug.Assert(((NormalSysExEvent)eventsReceived[0]).Data.SequenceEqual(new byte[] { 0x7F, 0x7F, 0x06, 0x03, 0xF7 }));
        Debug.Assert(((NormalSysExEvent)eventsReceived[1]).Data.SequenceEqual(new byte[] { 0x7F, 0x7F, 0x06, 0x01, 0xF7 }));
        Debug.Assert(((NormalSysExEvent)eventsReceived[2]).Data.SequenceEqual(new byte[] { 0x7F, 0x7F, 0x06, 0x44, 0x06, 0x01, 0x21, 0x02, 0x00, 0x00, 0x00, 0xF7 }));
        Debug.Assert(((NormalSysExEvent)eventsReceived[3]).Data.SequenceEqual(new byte[] { 0x7F, 0x7F, 0x06, 0x40, 0x03, 0x46, 0x01, 0x0A, 0xF7 }));
    }
}

